/* Class TorcCamera
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

// Qt
#include <QFile>

// Torc
#include "torclogging.h"
#include "torccameraoutput.h"
#include "torccamera.h"

TorcCameraParams::TorcCameraParams(void)
  : m_valid(false),
    m_width(0),
    m_height(0),
    m_stride(0),
    m_sliceHeight(0),
    m_frameRate(1),
    m_bitrate(0),
    m_timebase(VIDEO_TIMEBASE),
    m_segmentLength(0),
    m_gopSize(0),
    m_videoCodec(),
    m_contentDir()
{
}

TorcCameraParams::TorcCameraParams(const QVariantMap &Details)
  : m_valid(false),
    m_width(0),
    m_height(0),
    m_stride(0),
    m_sliceHeight(0),
    m_frameRate(1),
    m_bitrate(0),
    m_timebase(VIDEO_TIMEBASE),
    m_segmentLength(0),
    m_gopSize(0),
    m_videoCodec(),
    m_contentDir()
{
    if (!Details.contains("width") || !Details.contains("height"))
        return;

    bool bitrate   = Details.contains("bitrate");
    bool framerate = Details.contains("framerate");
    bool video     = bitrate && framerate;
    bool stills    = !bitrate && !framerate;

    if (!video && !stills)
        return;

    m_width     = Details.value("width").toInt();
    m_height    = Details.value("height").toInt();

    // N.B. pitch and slice height may be further constrained in certain encoders and may be further adjusted
    // most h264 streams will expect 16 pixel aligned video so round up
    int pitch = (m_width + 15) & ~15;
    if (pitch != m_width)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Rounding video width up to %1 from %2").arg(pitch).arg(m_width));
        m_width = pitch;
    }

    // and 8 pixel macroblock height
    int height = (m_height + 7) & ~7;
    if (height != m_height)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Rounding video height up to %1 from %2").arg(height).arg(m_height));
        m_height = height;
    }

    m_stride      = m_width;
    m_sliceHeight = m_height;

    bool forcemin = m_width < VIDEO_WIDTH_MIN || m_height < VIDEO_HEIGHT_MIN;
    bool forcemax = m_width > VIDEO_WIDTH_MAX || m_height > VIDEO_HEIGHT_MAX;
    if (forcemin)
    {
        m_width  = VIDEO_WIDTH_MIN;
        m_height = VIDEO_HEIGHT_MIN;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video too small - forcing output to %1x%2").arg(m_width).arg(m_height));
    }
    else if (forcemax)
    {
        m_width  = VIDEO_WIDTH_MAX;
        m_height = VIDEO_HEIGHT_MAX;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video too large - forcing output to %1x%2").arg(m_width).arg(m_height));
    }

    if (video)
    {
        m_frameRate = Details.value("framerate").toInt();
        m_bitrate   = Details.value("bitrate").toInt();

        if (m_frameRate < VIDEO_FRAMERATE_MIN)
        {
            m_frameRate = VIDEO_FRAMERATE_MIN;
            LOG(VB_GENERAL, LOG_WARNING, QString("Video framerate too low - forcing to %1").arg(m_frameRate));
        }
        else if (m_frameRate > VIDEO_FRAMERATE_MAX)
        {
            m_frameRate = VIDEO_FRAMERATE_MAX;
            LOG(VB_GENERAL, LOG_WARNING, QString("Video framerate too high - forcing to %1").arg(m_frameRate));
        }

        if (m_bitrate < VIDEO_BITRATE_MIN)
        {
            m_bitrate = VIDEO_BITRATE_MIN;
            LOG(VB_GENERAL, LOG_WARNING, QString("Video bitrate too low - forcing to %1").arg(m_bitrate));
        }
        else if (m_bitrate > VIDEO_BITRATE_MAX)
        {
            m_bitrate = VIDEO_BITRATE_MAX;
            LOG(VB_GENERAL, LOG_WARNING, QString("Video bitrate too high - forcing to %1").arg(m_bitrate));
        }

        m_segmentLength = m_frameRate * VIDEO_SEGMENT_TARGET;
        m_gopSize       = m_frameRate * VIDEO_GOPDURA_TARGET;

        LOG(VB_GENERAL, LOG_INFO, QString("Segment length: %1frames %2seconds").arg(m_segmentLength).arg(m_segmentLength / m_frameRate));
        LOG(VB_GENERAL, LOG_INFO, QString("GOP     length: %1frames %2seconds").arg(m_gopSize).arg(m_gopSize / m_frameRate));
        LOG(VB_GENERAL, LOG_INFO, QString("Camera video  : %1x%2@%3fps bitrate %4").arg(m_width).arg(m_height).arg(m_frameRate).arg(m_bitrate));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Camera stills: %1x%2").arg(m_width).arg(m_height));
    }

    m_valid = true;
}

TorcCameraParams& TorcCameraParams::operator =(const TorcCameraParams &Other)
{
    if (&Other != this)
    {
        this->m_valid         = Other.m_valid;
        this->m_width         = Other.m_width;
        this->m_height        = Other.m_height;
        this->m_stride        = Other.m_stride;
        this->m_sliceHeight   = Other.m_sliceHeight;
        this->m_frameRate     = Other.m_frameRate;
        this->m_bitrate       = Other.m_bitrate;
        this->m_timebase      = Other.m_timebase;
        this->m_segmentLength = Other.m_segmentLength;
        this->m_gopSize       = Other.m_gopSize;
        this->m_videoCodec    = Other.m_videoCodec;
        this->m_contentDir    = Other.m_contentDir;
    }
    return *this;
}

bool TorcCameraParams::operator == (const TorcCameraParams &Other) const
{
    return this->m_valid         == Other.m_valid &&
           this->m_width         == Other.m_width &&
           this->m_height        == Other.m_height &&
           this->m_stride        == Other.m_stride &&
           this->m_sliceHeight   == Other.m_sliceHeight &&
           this->m_frameRate     == Other.m_frameRate &&
           this->m_bitrate       == Other.m_bitrate &&
           this->m_timebase      == Other.m_timebase &&
           this->m_segmentLength == Other.m_segmentLength &&
           this->m_gopSize       == Other.m_gopSize;
           // ignore codec - it is set by the camera device
           //this->m_videoCodec    == Other.m_videoCodec;
           //this->m_contentDir    == Other.m_contentDir;
}

TorcCameraParams TorcCameraParams::Combine(const TorcCameraParams &Add)
{
    if (!Add.m_valid)
        return *this;
    if (!m_valid)
        return Add;
    if (Add == *this)
        return *this;

    if (!IsCompatible(Add))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error combining camera parameters - %1x%2 != %3x%4")
                                    .arg(m_width).arg(m_height).arg(Add.m_width).arg(Add.m_height));
        // this should cover most cases
        if ((m_width > Add.m_width) || (m_height > Add.m_height))
        {
            m_width  = Add.m_width;
            m_height = Add.m_height;
        }
        LOG(VB_GENERAL, LOG_WARNING, QString("Using smaller image size %1x%2").arg(m_width).arg(m_height));
    }

    // add video data
    if (IsStill() && Add.IsVideo())
    {
        m_bitrate       = Add.m_bitrate;
        m_frameRate     = Add.m_frameRate;
        m_gopSize       = Add.m_gopSize;
        m_stride        = Add.m_stride;
        m_sliceHeight   = Add.m_sliceHeight;
        m_segmentLength = Add.m_segmentLength;
        m_videoCodec    = Add.m_videoCodec;
        m_timebase      = Add.m_timebase;
        LOG(VB_GENERAL, LOG_INFO, "Added video to camera parameters");
    }
    // add stills
    else if (IsVideo() && Add.IsStill())
    {
        m_contentDir    = Add.m_contentDir;
    }

    return *this;
}

bool TorcCameraParams::IsVideo(void) const
{
    return m_valid && (m_frameRate > 1) && (m_bitrate > 0);
}

bool TorcCameraParams::IsStill(void) const
{
    return m_valid && (m_frameRate < 2) && (m_bitrate < 1) && !m_contentDir.isEmpty();
}

bool TorcCameraParams::IsCompatible(const TorcCameraParams &Other) const
{
    return m_valid && Other.m_valid && (Other.m_width == m_width) && (Other.m_height == m_height);
}

TorcCameraDevice::TorcCameraDevice(const TorcCameraParams &Params)
  : QObject(),
    m_params(Params),
    m_muxer(nullptr),
    m_videoStream(0),
    m_frameCount(0),
    m_haveInitSegment(false),
    m_bufferedPacket(nullptr),
    m_ringBuffer(nullptr),
    m_ringBufferLock(QReadWriteLock::Recursive),
    m_referenceTime(0),
    m_discardDrift(2),
    m_shortAverage(VIDEO_DRIFT_SHORT / VIDEO_SEGMENT_TARGET),
    m_longAverage(VIDEO_DRIFT_LONG / VIDEO_SEGMENT_TARGET),
    m_stillsRequired(0),
    m_stillsExpected(0),
    m_stillsBuffers()
{
}

TorcCameraDevice::~TorcCameraDevice()
{
    if (m_muxer)
    {
        m_muxer->Finish();
        delete m_muxer;
    }

    if (m_ringBuffer)
        delete m_ringBuffer;

    if (m_bufferedPacket)
        av_packet_free(&m_bufferedPacket);

    ClearStillsBuffers();
}

bool TorcCameraDevice::Setup(void)
{
    QWriteLocker locker(&m_ringBufferLock);
    int buffersize = (m_params.m_bitrate * (m_params.m_segmentLength / m_params.m_frameRate) * VIDEO_SEGMENT_NUMBER) / 8;
    m_ringBuffer   = new TorcSegmentedRingBuffer(buffersize, VIDEO_SEGMENT_MAX);
    m_muxer        = new TorcMuxer(m_ringBuffer);
    if (!m_muxer)
        return false;

    m_videoStream = m_muxer->AddH264Stream(m_params.m_width, m_params.m_height, VIDEO_H264_PROFILE, m_params.m_bitrate);
    if (!m_muxer->IsValid())
        return false;

    connect(m_ringBuffer, &TorcSegmentedRingBuffer::SegmentReady,     this, &TorcCameraDevice::SegmentReady);
    connect(m_ringBuffer, &TorcSegmentedRingBuffer::SegmentRemoved,   this, &TorcCameraDevice::SegmentRemoved);
    connect(m_ringBuffer, &TorcSegmentedRingBuffer::InitSegmentReady, this, &TorcCameraDevice::InitSegmentReady);

    m_frameCount      = 0;
    m_bufferedPacket  = nullptr;
    m_haveInitSegment = false;
    return true;
}

QByteArray TorcCameraDevice::GetSegment(int Segment)
{
    QReadLocker locker(&m_ringBufferLock);
    if (m_ringBuffer)
        return m_ringBuffer->GetSegment(Segment);
    return QByteArray();
}

QByteArray TorcCameraDevice::GetInitSegment(void)
{
    QReadLocker locker(&m_ringBufferLock);
    if (m_ringBuffer)
        return m_ringBuffer->GetInitSegment();
    return QByteArray();
}

/*! \brief Tell the camera to take Count number of still images
 *
 * \note We ignore values below the current setting, as the stills will be triggered when the input
 *       value moves from zero to X - but it should then drop back to zero.
*/
void TorcCameraDevice::TakeStills(uint Count)
{
    if (Count > m_stillsRequired)
    {
        bool started = m_stillsExpected > 0;
        (void)EnableStills(Count);
        if (!started)
            StartStill();
    }
}

bool TorcCameraDevice::EnableStills(uint Count)
{
    m_stillsRequired  = Count;
    m_stillsExpected  = Count;
    return true;
}

void TorcCameraDevice::SaveStill(void)
{
    if (m_stillsExpected)
    {
        if (m_stillsRequired)
        {
            if (!m_params.m_contentDir.isEmpty())
            {
                QFile file(m_params.m_contentDir + QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss_zzz") + ".jpg");
                if (file.open(QIODevice::ReadWrite | QIODevice::Truncate))
                {
                    QPair<quint32, uint8_t*> pair;
                    foreach(pair, m_stillsBuffers)
                        file.write((const char*)pair.second, pair.first);
                    file.close();
                    LOG(VB_GENERAL, LOG_INFO, QString("Saved snapshot as '%1'").arg(file.fileName()));
                    emit StillReady(file.fileName());
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Failed to open %1 for writing").arg(file.fileName()));
                }
            }
            m_stillsRequired--;
        }
        m_stillsExpected--;
    }
    ClearStillsBuffers();
}

void TorcCameraDevice::SaveStillBuffer(quint32 Length, uint8_t *Data)
{
    if (Length < 1 || !Data)
        return;

    m_stillsBuffers.append(QPair<quint32,uint8_t*>(Length, Data));
}

void TorcCameraDevice::ClearStillsBuffers(void)
{
    QPair<quint32, uint8_t*> buffer;
    foreach(buffer, m_stillsBuffers)
        free(buffer.second);
    m_stillsBuffers.clear();
}

void TorcCameraDevice::TrackDrift(void)
{
    quint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_referenceTime)
        m_referenceTime = now;

    // discard first few samples while camera settles down
    if (m_discardDrift)
    {
        m_discardDrift--;
        return;
    }

    // reference time points to the end of the first video segment
    m_ringBufferLock.lockForRead();
    int last = m_ringBuffer->GetHead();
    m_ringBufferLock.unlock();
    if (last < 0)
        return;

    // all milliseconds
    qint64 drift = QDateTime::currentMSecsSinceEpoch() - (m_referenceTime + (last * VIDEO_SEGMENT_TARGET * 1000));
    double shortaverage = m_shortAverage.AddValue(drift);
    double longaverage  = m_longAverage.AddValue(drift);

    static const double timedelta = ((VIDEO_DRIFT_LONG - VIDEO_DRIFT_SHORT) / 2) * 1000;
    double driftdelta = longaverage - shortaverage;
    double gradient   = driftdelta / timedelta;
    double timetozero = qAbs(driftdelta) < 1 ? qInf() : drift / gradient;

    LOG(VB_GENERAL, LOG_INFO, QString("Drift: %1secs 1min %2 5min %3 timetozero %4")
        .arg((double)drift/1000, 0, 'f', 3).arg(shortaverage/1000, 0, 'f', 3).arg(longaverage/1000, 0, 'f', 3).arg(timetozero/1000, 0, 'f', 3));
}

TorcCameraFactory* TorcCameraFactory::gTorcCameraFactory = nullptr;

TorcCameraFactory::TorcCameraFactory()
  : nextTorcCameraFactory(gTorcCameraFactory)
{
    qRegisterMetaType<TorcCameraParams>("TorcCameraParams&");
    gTorcCameraFactory = this;
}

TorcCameraFactory* TorcCameraFactory::GetTorcCameraFactory(void)
{
    return gTorcCameraFactory;
}

TorcCameraFactory* TorcCameraFactory::NextFactory(void) const
{
    return nextTorcCameraFactory;
}

TorcCameraDevice* TorcCameraFactory::GetCamera(const QString &Type, const TorcCameraParams &Params)
{
    TorcCameraDevice* result = nullptr;

    TorcCameraFactory* factory = TorcCameraFactory::GetTorcCameraFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        if (factory->CanHandle(Type, Params))
        {
            result = factory->Create(Type, Params);
            break;
        }
    }

    return result;
}
