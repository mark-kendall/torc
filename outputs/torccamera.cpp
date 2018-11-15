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

// Torc
#include "torclogging.h"
#include "torccameraoutput.h"
#include "torccamera.h"

#define BLACKLIST QString("StartCamera,StopCamera,WritingStarted,WritingStopped")

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
    m_model(),
    m_videoCodec()
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
    m_model(),
    m_videoCodec()
{
    if (!Details.contains("width")     || !Details.contains("height")  ||
        !Details.contains("framerate") || !Details.contains("bitrate") ||
        !Details.contains("model"))
    {
        return;
    }

    m_width     = Details.value("width").toInt();
    m_height    = Details.value("height").toInt();
    m_frameRate = Details.value("framerate").toInt();
    m_bitrate   = Details.value("bitrate").toInt();
    m_model     = Details.value("model").toString();

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

    m_valid = true;
}

TorcCameraParams::TorcCameraParams(const TorcCameraParams &Other)
  : m_valid(Other.m_valid),
    m_width(Other.m_width),
    m_height(Other.m_height),
    m_stride(Other.m_stride),
    m_sliceHeight(Other.m_sliceHeight),
    m_frameRate(Other.m_frameRate),
    m_bitrate(Other.m_bitrate),
    m_timebase(Other.m_timebase),
    m_segmentLength(Other.m_segmentLength),
    m_gopSize(Other.m_gopSize),
    m_model(Other.m_model),
    m_videoCodec(Other.m_videoCodec)
{
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
        this->m_model         = Other.m_model;
        this->m_videoCodec    = Other.m_videoCodec;
    }
    return *this;
}

TorcCameraDevice::TorcCameraDevice(const TorcCameraParams &Params)
  : QObject(),
    m_params(Params),
    m_ringBuffer(NULL)
{
}

TorcCameraDevice::~TorcCameraDevice()
{
    if (m_ringBuffer)
    {
        delete m_ringBuffer;
        m_ringBuffer = NULL;
    }
}

QByteArray TorcCameraDevice::GetSegment(int Segment)
{
    if (m_ringBuffer)
        return m_ringBuffer->GetSegment(Segment);
    return QByteArray();
}

QByteArray TorcCameraDevice::GetInitSegment(void)
{
    if (m_ringBuffer)
        return m_ringBuffer->GetInitSegment();
    return QByteArray();
}

TorcCameraParams TorcCameraDevice::GetParams(void)
{
    return m_params;
}

TorcCameraThread::TorcCameraThread(TorcCameraOutput *Parent, const QString &Type, const TorcCameraParams &Params)
  : TorcQThread("Camera"),
    m_parent(Parent),
    m_type(Type),
    m_params(Params),
    m_camera(NULL),
    m_cameraLock(QReadWriteLock::Recursive),
    m_stop(false)
{
    if (m_parent)
    {
        connect(this, SIGNAL(WritingStarted()), m_parent, SLOT(WritingStarted()));
        connect(this, SIGNAL(WritingStopped()), m_parent, SLOT(WritingStopped()));
    }
}

TorcCameraThread::~TorcCameraThread()
{
    QWriteLocker locker(&m_cameraLock);
    if (m_camera)
    {
        delete m_camera;
        m_camera = NULL;
    }
}

void TorcCameraThread::StopWriting(void)
{
    m_stop = true;
}

void TorcCameraThread::Start(void)
{
    if (m_stop)
        return;

    LOG(VB_GENERAL, LOG_INFO, "Camera thread starting");

    m_cameraLock.lockForWrite();
    m_camera = TorcCameraFactory::GetCamera(m_type, m_params);

    if (m_camera)
    {
        if (m_parent)
        {
            connect(m_camera, SIGNAL(InitSegmentReady()),  m_parent, SLOT(InitSegmentReady()));
            connect(m_camera, SIGNAL(SegmentReady(int)),   m_parent, SLOT(SegmentReady(int)));
            connect(m_camera, SIGNAL(SegmentRemoved(int)), m_parent, SLOT(SegmentRemoved(int)));
            connect(m_camera, SIGNAL(SetErrored(bool)),    m_parent, SLOT(CameraErrored(bool)));
        }

        if (m_camera->Setup())
        {
            m_cameraLock.unlock();
            emit WritingStarted();
            m_cameraLock.lockForRead();
            while (!m_stop)
            {
                if (!m_camera->WriteFrame())
                {
                    LOG(VB_GENERAL, LOG_WARNING, "Camera returned error from WriteFrame()");
                    break;
                }
            }
            m_cameraLock.unlock();
            m_cameraLock.lockForWrite();
            m_camera->Stop();
        }

        m_camera->disconnect();
        delete m_camera;
        m_camera = NULL;
    }

    m_cameraLock.unlock();

    LOG(VB_GENERAL, LOG_INFO, "Camera thread stopping");
    emit WritingStopped();
}

void TorcCameraThread::Finish(void)
{
}

QByteArray TorcCameraThread::GetSegment(int Segment)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetSegment(Segment);
    return QByteArray();
}

QByteArray TorcCameraThread::GetInitSegment(void)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetInitSegment();
    return QByteArray();
}

TorcCameraParams TorcCameraThread::GetParams(void)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetParams();
    return TorcCameraParams();
}

TorcCameraFactory* TorcCameraFactory::gTorcCameraFactory = NULL;

TorcCameraFactory::TorcCameraFactory()
  : nextTorcCameraFactory(gTorcCameraFactory)
{
    gTorcCameraFactory = this;
}

TorcCameraFactory::~TorcCameraFactory()
{
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
    TorcCameraDevice* result = NULL;

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
