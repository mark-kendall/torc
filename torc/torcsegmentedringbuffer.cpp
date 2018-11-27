/* Class TorcSegmentedRingBuffer
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torcsegmentedringbuffer.h"

/*! \class TorcSegmentedRingBuffer
 *
 * A circular buffer customised for storing segmented media data - principally fragmented
 * MP4 files but should work with other formats such as transport streams (TS).
 *
 * The owner is responsible for ensuring the total buffer size is appropriate for the use case
 * (i.e. media type/bitrate and buffering required).
*/
TorcSegmentedRingBuffer::TorcSegmentedRingBuffer(int Size, int MaxSegments)
  : m_size(Size),
    m_data(Size, '0'),
    m_readPosition(0),
    m_writePosition(1), // NB avoid read == write
    m_currentSize(0),
    m_currentStartPosition(1),
    m_segmentsLock(QReadWriteLock::Recursive),
    m_segments(),
    m_segmentRefs(),
    m_segmentCounter(0),
    m_maxSegments(MaxSegments),
    m_initSegment()
{
    LOG(VB_GENERAL, LOG_INFO, QString("Allocated segmented ring buffer of size %1bytes").arg(m_size));
}

TorcSegmentedRingBuffer::~TorcSegmentedRingBuffer()
{
    QReadLocker locker1(&m_segmentsLock);
    m_segments.clear();
    m_segmentRefs.clear();
}

/// Return the number of free bytes available for writing.
inline int TorcSegmentedRingBuffer::GetBytesFree(void)
{
    int result = 0;
    m_segmentsLock.lockForRead();
    result = (m_readPosition - m_writePosition) - 1;
    if (result < 0)
        result += m_size;
    m_segmentsLock.unlock();
    return result;
}

/// Return the size of the buffer (NOT the number of segments).
int TorcSegmentedRingBuffer::GetSize(void)
{
    return m_size;
}

/// Return the number of the segment at the head of the queue (the newest).
int TorcSegmentedRingBuffer::GetHead(void)
{
    int result = -1;
    m_segmentsLock.lockForRead();
    if (!m_segments.isEmpty())
        result = m_segmentRefs.last();
    m_segmentsLock.unlock();
    return result;
}

/*! \brief Return the number of available segments
 *
 * If the buffer is not empty, TailRef will be set to the value of the oldest segment.
*/
int TorcSegmentedRingBuffer::GetSegmentsAvail(int &TailRef)
{
    // only the 'writer' can change the number of segments
    int result = 0;
    m_segmentsLock.lockForRead();
    result = m_segments.size();
    if (result > 0)
        TailRef = m_segmentRefs.first();
    m_segmentsLock.unlock();
    return result;
}

/*! \brief Finish the current segment and start a new one
 *
 * The buffer's owner is responsible for indicating that the segment is finished.
 *
 * \param Init Indicates that this is an MP4 'init' segment that should be saved separately.
*/
int TorcSegmentedRingBuffer::FinishSegment(bool Init)
{
    QWriteLocker locker(&m_segmentsLock);
    int result = -1;
    if (m_writePosition == m_currentStartPosition || m_currentSize < 1)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Cannot finish segment - nothing written");
        return result;
    }

    //LOG(VB_GENERAL, LOG_INFO, QString("Finished segmentref %1 start %2 size %3 (segments %4)")
    //    .arg(m_segmentCounter).arg(m_currentStartPosition).arg(m_currentSize).arg(m_segments.size() + 1));

    result = m_segmentCounter;
    m_segments.enqueue(QPair<int,int>(m_currentStartPosition, m_currentSize));
    m_segmentRefs.enqueue(m_segmentCounter);
    m_currentStartPosition = m_writePosition;
    m_currentSize = 0;
    m_segmentCounter++;
    if (Init)
        SaveInitSegment();
    else
        emit SegmentReady(result);
    return result;
}

/// Write data to the current segment.
int TorcSegmentedRingBuffer::Write(const uint8_t *Data, int Size)
{
    if (!Data || Size <= 0 || Size >= m_size)
        return -1;

    int dummyref = 0;
    // free up space if needed
    while ((GetBytesFree() < Size) || (GetSegmentsAvail(dummyref) > m_maxSegments))
    {
        if (GetSegmentsAvail(dummyref) < 1)
        {
            LOG(VB_GENERAL, LOG_WARNING, "Segmented buffer underrun - no space for write");
            return -1;
        }

        m_segmentsLock.lockForWrite();
        QPair<int,int> segment = m_segments.dequeue();
        int removed = m_segmentRefs.dequeue();
        m_readPosition = (segment.first + segment.second) % m_size;
        m_segmentsLock.unlock();
        emit SegmentRemoved(removed);
    }

    int copy = qMin(Size, m_size - m_writePosition);
    memcpy(m_data.data() + m_writePosition, Data, copy);
    if (copy < Size) // wrap
        memcpy(m_data.data(), Data + copy, Size - copy);
    m_writePosition = (m_writePosition + Size) % m_size;
    m_currentSize  += Size;
    return Size;
}

/*! \brief Write data to the current segment
 * \overload int TorcSegmentedRingBuffer::Write(const uint8_t *Data, int Size)
*/
int TorcSegmentedRingBuffer::Write(QByteArray *Data, int Size)
{
    if (Data)
        return Write((uint8_t*)Data->constData(), Size);
    return 0;
}

/*! \brief Copy Max bytes of a segment identified by SegmentRef into the memory pointed to at Dst
 *
 *  If returned value == MAX, try again with offset
 *
 *  \returns -1 on error and the number of bytes copied/read on sucess (0 indicates no more bytes to read)
*/
int TorcSegmentedRingBuffer::ReadSegment(uint8_t* Dst, int Max, int SegmentRef, int Offset /*=0*/)
{
    if (!Dst || Max <= 0 || Max > m_size || SegmentRef < 0 || Offset < 0 || Offset > m_size)
        return -1;

    QReadLocker locker1(&m_segmentsLock);

    if (!m_segmentRefs.contains(SegmentRef))
        return -1;

    QPair<int,int> segment = m_segments[m_segmentRefs.indexOf(SegmentRef)];
    if (Offset >= segment.second)
        return -1;

    int size = qMin(Max, segment.second - Offset);
    if (size < 1)
        return 0;
    int readpos = (segment.first + Offset) % m_size;
    int read    = qMin(size, m_size - readpos);
    memcpy(Dst, m_data.data() + readpos, read);
    if (read < size)
        memcpy(Dst + read, m_data.data(), size - read);
    return size;
}

/*! \brief Copy the segment identified by SegmentRef to the IODevice specified by Dst
 *
 *  \returns -1 on error and the size of the copied/read segment on success.
*/
int TorcSegmentedRingBuffer::ReadSegment(QIODevice *Dst, int SegmentRef)
{
    if (!Dst || SegmentRef < 0)
        return -1;

    QReadLocker locker1(&m_segmentsLock);

    if (!m_segmentRefs.contains(SegmentRef))
        return -1;

    QPair<int,int> segment = m_segments[m_segmentRefs.indexOf(SegmentRef)];
    int read = qMin(segment.second, m_size - segment.first);
    Dst->write((const char*)(m_data.data() + segment.first), read);
    if (read < segment.second)
        Dst->write((const char*)m_data.data(), segment.second - read);
    return segment.second;
}

/// Save the MP4 'init' segment.
void TorcSegmentedRingBuffer::SaveInitSegment(void)
{
    QWriteLocker locker(&m_segmentsLock);
    if (m_segments.size() != 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot retrieve init segment - zero or >1 segments");
        return;
    }

    // sanity check
    if (!m_initSegment.isEmpty())
        LOG(VB_GENERAL, LOG_WARNING, "Already have init segment - overwriting");

    // copy the segment
    QPair<int,int> segment = m_segments.dequeue();
    m_initSegment = QByteArray(segment.second, '0');
    int read = qMin(segment.second, m_size - segment.first);
    memcpy(m_initSegment.data(), m_data.data() + segment.first, read);
    if (read < segment.second)
        memcpy(m_initSegment.data() + read, m_data.data(), segment.second - read);

    // reset the ringbuffer
    LOG(VB_GENERAL, LOG_INFO, QString("Init segment saved (%1 bytes) - resetting ringbuffer").arg(m_initSegment.size()));
    m_segments.clear();
    m_segmentRefs.clear();
    m_readPosition   = 0;
    m_writePosition  = 1;
    m_currentSize    = 0;
    m_currentStartPosition = 1;
    m_segmentCounter = 0;
    emit InitSegmentReady();
}

/// Return a copy of the segment identified by SegmentRef.
QByteArray TorcSegmentedRingBuffer::GetSegment(int SegmentRef)
{
    if (SegmentRef < 0)
        return QByteArray();

    QReadLocker locker1(&m_segmentsLock);

    if (!m_segmentRefs.contains(SegmentRef))
        return QByteArray();

    QPair<int,int> segment = m_segments[m_segmentRefs.indexOf(SegmentRef)];
    QByteArray result(segment.second, '0');
    int read = qMin(segment.second, m_size - segment.first);
    memcpy(result.data(), m_data.data() + segment.first, read);
    if (read < segment.second)
        memcpy(result.data() + read, m_data.data(), segment.second - read);
    return result;
}

/// Return a copy of the MP4 'init' segment.
QByteArray TorcSegmentedRingBuffer::GetInitSegment(void)
{
    QReadLocker locker(&m_segmentsLock);
    return m_initSegment;
}
