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
    m_videoCodec(Other.m_videoCodec),
    m_contentDir(Other.m_contentDir)
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
