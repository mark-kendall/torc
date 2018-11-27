/* Class TorcPiCamera
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
#include <QTime>
#include <QThread>

// Torc
#include "torclogging.h"
#include "torccentral.h"
#include "torcpicamera.h"
#include "torcdirectories.h"

// OpenMax
#include "OMX_Video.h"

// Broadcom
#include "bcm_host.h"
#include "OMX_Broadcom.h"

#define BROADCOM_CAMERA                "OMX.broadcom.camera"
#define BROADCOM_ENCODER               "OMX.broadcom.video_encode"
#define BROADCOM_NULLSINK              "OMX.broadcom.null_sink"
#define BROADCOM_VIDEOSPLITTER         "OMX.broadcom.video_splitter"
#define BROADCOM_IMAGEENCODER          "OMX.broadcom.image_encode"
#define CAMERA_SHARPNESS               0   // -100->100
#define CAMERA_CONTRAST                0   // -100->100
#define CAMERA_BRIGHTNESS              50  //  0->100
#define CAMERA_SATURATION              0   // -100->100
#define CAMERA_METERING                OMX_MeteringModeAverage
#define CAMERA_EXPOSURE                OMX_ExposureControlAuto
#define CAMERA_EXPOSURE_COMPENSATION   0   // -10->10
#define CAMERA_SHUTTER_SPEED_AUTO      OMX_TRUE
#define CAMERA_SHUTTER_SPEED           1.0/8.0
#define CAMERA_ISO_AUTO                OMX_TRUE
#define CAMERA_ISO                     100 // 100->800
#define CAMERA_WHITE_BALANCE           OMX_WhiteBalControlAuto
#define CAMERA_FRAME_STABILIZATION     OMX_FALSE
#define CAMERA_WHITE_BALANCE_RED_GAIN  1000
#define CAMERA_WHITE_BALANCE_BLUE_GAIN 1000
#define CAMERA_IMAGE_FILTER            OMX_ImageFilterNone
#define CAMERA_MIRROR                  OMX_MirrorNone
#define CAMERA_ROTATION                0
#define CAMERA_COLOR_ENABLE            OMX_FALSE
#define CAMERA_COLOR_U                 128
#define CAMERA_COLOR_V                 128
#define CAMERA_NOISE_REDUCTION         OMX_TRUE
#define CAMERA_ROI_TOP                 0
#define CAMERA_ROI_LEFT                0
#define CAMERA_ROI_WIDTH               100
#define CAMERA_ROI_HEIGHT              100
#define CAMERA_DRC                     OMX_DynRangeExpOff // Off/Low/Medium/High

#define ENCODER_QP                     OMX_FALSE
#define ENCODER_QP_I                   0
#define ENCODER_QP_P                   0
#define ENCODER_SEI                    OMX_FALSE
#define ENCODER_EEDE                   OMX_FALSE
#define ENCODER_EEDE_LOSS_RATE         0
#define ENCODER_INLINE_HEADERS         OMX_TRUE
#define ENCODER_SPS_TIMING             OMX_TRUE

bool TorcPiCamera::gPiCameraDetected = false;

TorcPiCamera::TorcPiCamera(const TorcCameraParams &Params)
  : TorcCameraDevice(Params),
    m_cameraType(Unknown),
    m_core(),
    m_camera((char*)BROADCOM_CAMERA),
    m_videoEncoder((char*)BROADCOM_ENCODER),
    m_nullSink((char*)BROADCOM_NULLSINK),
    m_splitter((char*)BROADCOM_VIDEOSPLITTER),
    m_imageEncoder((char*)BROADCOM_IMAGEENCODER),
    m_cameraPreviewPort(0),
    m_cameraVideoPort(0),
    m_videoEncoderInputPort(0),
    m_videoEncoderOutputPort(0),
    m_imageEncoderOutputPort(0),
    m_splitterTunnel(&m_camera, 1, OMX_IndexParamVideoInit, &m_splitter, 0, OMX_IndexParamVideoInit),
    m_videoTunnel(&m_splitter, 0, OMX_IndexParamVideoInit, &m_videoEncoder,  0, OMX_IndexParamVideoInit),
    m_previewTunnel(&m_camera, 0, OMX_IndexParamVideoInit, &m_nullSink, 0, OMX_IndexParamVideoInit),
    m_imageTunnel(&m_splitter, 1, OMX_IndexParamVideoInit, &m_imageEncoder,  0, OMX_IndexParamImageInit)
{
    if (m_params.m_frameRate > 30 && (m_params.m_width > 1280 || m_params.m_height > 720))
    {
        m_params.m_frameRate = 30;
        LOG(VB_GENERAL, LOG_INFO, "Restricting camera framerate to 30fps for full HD");
    }
    else if (m_params.m_frameRate > 60 && (m_params.m_width > 720 || m_params.m_height > 576))
    {
        m_params.m_frameRate = 60;
        LOG(VB_GENERAL, LOG_INFO, "Restricting camera framerate to 60fps for partial HD");
    }
    else if (m_params.m_frameRate > 90)
    {
        m_params.m_frameRate = 90;
        LOG(VB_GENERAL, LOG_INFO, "Resticting camera framerate to 90fps for SD");
    }

    // encoder requires stride to be divisible by 32 and slice height by 16
    m_params.m_stride      = (m_params.m_stride + 31) & ~31;
    m_params.m_sliceHeight = (m_params.m_sliceHeight + 15) & ~15;
    LOG(VB_GENERAL, LOG_INFO, QString("Camera encoder stride: %1 slice height: %2").arg(m_params.m_stride).arg(m_params.m_sliceHeight));
}

TorcPiCamera::~TorcPiCamera()
{
}

void TorcPiCamera::StreamVideo(bool Video)
{
    (void)Video;
}

bool TorcPiCamera::Setup(void)
{
    // NB Pi hardware is initialised in TorcPiGPIO as it is static

    if (!m_camera.IsValid() || !m_videoEncoder.IsValid() || !m_nullSink.IsValid() || !m_splitter.IsValid() ||
        !m_imageEncoder.IsValid())
        return false;

    // load drivers - Broadcom specific
    if (!LoadDrivers())
        return false;

    // retrieve port numbers
    m_cameraPreviewPort      = m_camera.GetPort(OMX_DirOutput,  0, OMX_IndexParamVideoInit);
    m_cameraVideoPort        = m_camera.GetPort(OMX_DirOutput,  1, OMX_IndexParamVideoInit);
    m_videoEncoderInputPort  = m_videoEncoder.GetPort(OMX_DirInput,  0, OMX_IndexParamVideoInit);
    m_videoEncoderOutputPort = m_videoEncoder.GetPort(OMX_DirOutput, 0, OMX_IndexParamVideoInit);
    m_imageEncoderOutputPort = m_imageEncoder.GetPort(OMX_DirOutput, 0, OMX_IndexParamImageInit);

    if (!m_cameraVideoPort || !m_cameraPreviewPort || !m_videoEncoderInputPort || !m_videoEncoderOutputPort || !m_imageEncoderOutputPort)
        return false;

    // configure camera
    if (!ConfigureCamera())
        return false;

    // configure encoders
    if (!ConfigureVideoEncoder() || !ConfigureImageEncoder())
        return false;

    // create tunnels
    if (m_splitterTunnel.Create() || m_previewTunnel.Create() || m_videoTunnel.Create() || m_imageTunnel.Create())
        return false;

    // set to idle
    m_camera.SetState(OMX_StateIdle);
    m_splitter.SetState(OMX_StateIdle);
    m_videoEncoder.SetState(OMX_StateIdle);
    m_imageEncoder.SetState(OMX_StateIdle);
    m_nullSink.SetState(OMX_StateIdle);

    // set splitter to image encoder to single shot
    // there is no setting for nothing - it is either streamed (0) or 1 or more single frames
    // so request 1 frame and then ignore it
    if (!EnableStills(1))
        return false;
    m_stillsRequired = 0;

    // TODO add error checking to these calls
    // enable ports
    m_camera.EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit);
    m_camera.EnablePort(OMX_DirOutput, 1, true, OMX_IndexParamVideoInit);
    m_splitter.EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);
    m_splitter.EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit);
    m_splitter.EnablePort(OMX_DirOutput, 1, true, OMX_IndexParamVideoInit);
    m_videoEncoder.EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);
    m_videoEncoder.EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit, false);
    m_videoEncoder.CreateBuffers(OMX_DirOutput, 0, OMX_IndexParamVideoInit, this);
    m_videoEncoder.WaitForResponse(OMX_CommandPortEnable, m_videoEncoderOutputPort, 1000);
    m_imageEncoder.EnablePort(OMX_DirInput, 0, true, OMX_IndexParamImageInit);
    m_imageEncoder.EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamImageInit, false);
    m_imageEncoder.CreateBuffers(OMX_DirOutput, 0, OMX_IndexParamImageInit, this);
    m_imageEncoder.WaitForResponse(OMX_CommandPortEnable, m_imageEncoderOutputPort, 1000);
    m_nullSink.EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);


    // set state to execute
    m_camera.SetState(OMX_StateExecuting);
    m_splitter.SetState(OMX_StateExecuting);
    m_videoEncoder.SetState(OMX_StateExecuting);
    m_imageEncoder.SetState(OMX_StateExecuting);
    m_nullSink.SetState(OMX_StateExecuting);

    // enable
    OMX_CONFIG_PORTBOOLEANTYPE capture;
    OMX_INITSTRUCTURE(capture);
    capture.nPortIndex = m_cameraVideoPort;
    capture.bEnabled   = OMX_TRUE;
    if (m_camera.SetConfig(OMX_IndexConfigPortCapturing, &capture))
        return false;

    if (TorcCameraDevice::Setup())
    {
        LOG(VB_GENERAL, LOG_INFO, "Pi camera setup");
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to setup Pi camera");
    return false;
}

/*! \brief Start the camera
 *
 * The camera device is event driven but we must kick off by asking for output
 * buffers to be filled.
 */
bool TorcPiCamera::Start(void)
{
    // there should be one still pipelined. We need to flush it.
    StartStill();

    // video is currently just on...
    StartVideo();

    return true;
}

/*! \brief An OpenMax buffer is ready (full)
 *
 * This method is signalled when a buffer becomes available - for our use case, this is
 * after TorcOMXComponent::FillThisBuffer has completed.
*/
void TorcPiCamera::BufferReady(OMX_BUFFERHEADERTYPE* Buffer, quint64 Type)
{
    if (!Buffer)
        return;

    // Stills

    // m_stillsExpected is always >= m_stillsRequired
    if (m_stillsExpected && ((OMX_INDEXTYPE)Type == OMX_IndexParamImageInit) && (m_imageEncoder.GetAvailableBuffers(OMX_DirOutput, 0, OMX_IndexParamImageInit) > 0))
    {
        ProcessStillsBuffer(Buffer);
        return;
    }

    // Video
    if (((OMX_INDEXTYPE)Type == OMX_IndexParamVideoInit) && m_videoEncoder.GetAvailableBuffers(OMX_DirOutput, 0, OMX_IndexParamVideoInit) > 0)
    {
        ProcessVideoBuffer(Buffer);
        return;
    }
}

/*! \brief Start capturing a still image buffer.
 *
 * \note This SHOULD be called at the start of stills capture or when the previous
 *       still has been completed. In both cases a buffer should be available and this
 *       method will then be non-blocking.
 */
void TorcPiCamera::StartStill(void)
{
    OMX_BUFFERHEADERTYPE* buffer = m_imageEncoder.GetBuffer(OMX_DirOutput, 0, 1000, OMX_IndexParamImageInit);
    if (buffer)
        (void)m_imageEncoder.FillThisBuffer(buffer);

    // else panic...

}

void TorcPiCamera::ProcessStillsBuffer(OMX_BUFFERHEADERTYPE *Buffer)
{
    if (!Buffer)
        return;

    if (Buffer->nFilledLen > 0)
    {
        uint8_t* data = (uint8_t*)malloc(Buffer->nFilledLen);
        memcpy(data, Buffer->pBuffer + Buffer->nOffset, Buffer->nFilledLen);
        SaveStillBuffer(Buffer->nFilledLen, data);
    }

    if (Buffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME)
        SaveStill();

    if (m_stillsExpected)
        StartStill();
}

bool TorcPiCamera::EnableVideo(bool Video)
{
    (void)Video;
    return true;
}

/*! \brief Start capturing a video buffer.
 *
 * \note This SHOULD be called at the start of video capture when a buffer should be available and this
 *       method will then be non-blocking.
 */
void TorcPiCamera::StartVideo(void)
{
    OMX_BUFFERHEADERTYPE* buffer = m_videoEncoder.GetBuffer(OMX_DirOutput, 0, 1000, OMX_IndexParamVideoInit);
    if (buffer)
        (void)m_videoEncoder.FillThisBuffer(buffer);
}

void TorcPiCamera::ProcessVideoBuffer(OMX_BUFFERHEADERTYPE *Buffer)
{
    if (!Buffer || !m_muxer)
        return;

    bool idr      = Buffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME;
    bool complete = Buffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME;
    bool sps      = Buffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG;

    if (!sps && complete)
        m_frameCount++;

    // TODO wrap pts
    // this increases the pts monotonically based on actual complete frames received
    qint64 pts = m_frameCount * ((float)m_params.m_timebase / (float)m_params.m_frameRate);
    // sps is not considered a frame but muxer will complain if the pts does not increase
    if (sps)
        pts++;

    // NB we use av_packet_from_data and av_grow_packet for the buffered packet - which uses the internal AVBufferRef structure
    // Do not manually release AVPacket->data
    if (m_bufferedPacket)
    {
        quint32 offset = m_bufferedPacket->size;
        av_grow_packet(m_bufferedPacket, Buffer->nFilledLen);
        memcpy(m_bufferedPacket->data + offset, reinterpret_cast<uint8_t*>(Buffer->pBuffer) + Buffer->nOffset, Buffer->nFilledLen);
    }

    if (complete)
    {
        AVPacket *packet = m_bufferedPacket;
        if (!packet)
        {
            packet = av_packet_alloc();
            av_init_packet(packet);
            packet->size = Buffer->nFilledLen;
            packet->data = reinterpret_cast<uint8_t*>(Buffer->pBuffer) + Buffer->nOffset;
        }

        packet->stream_index = m_videoStream;
        packet->pts          = pts;
        packet->dts          = pts;
        packet->duration     = 1;
        packet->flags       |= idr ? AV_PKT_FLAG_KEY : 0;

        {
            QWriteLocker locker(&m_ringBufferLock); // muxer accesses ringbuffer
            m_muxer->AddPacket(packet, sps);

            if (sps && !m_haveInitSegment)
            {
                QByteArray config = QByteArray::fromRawData((char*)packet->data, packet->size);
                m_params.m_videoCodec = m_muxer->GetAVCCodec(config);
                m_haveInitSegment = true;
                m_muxer->FinishSegment(true);
                // video is notionally available once the init segment is available
                emit WritingStarted();
            }
            else if (!sps && !(m_frameCount % (m_params.m_frameRate * VIDEO_SEGMENT_TARGET)))
            {
                m_muxer->FinishSegment(false);
                locker.unlock();
                TrackDrift();
                locker.relock();
            }

            if (m_bufferedPacket)
            {
                av_packet_free(&m_bufferedPacket);
                m_bufferedPacket = NULL;
            }
            else
            {
                packet->data = NULL;
                av_packet_free(&packet);
            }
        }
    }
    else
    {
        if (!m_bufferedPacket)
        {
            uint8_t* data = (uint8_t*)av_malloc(Buffer->nFilledLen);
            m_bufferedPacket = av_packet_alloc();
            av_init_packet(m_bufferedPacket);
            av_packet_from_data(m_bufferedPacket, data, Buffer->nFilledLen);
            memcpy(m_bufferedPacket->data, reinterpret_cast<uint8_t*>(Buffer->pBuffer) + Buffer->nOffset, Buffer->nFilledLen);
            m_bufferedPacket->flags |= idr ? AV_PKT_FLAG_KEY : 0;
        }
    }

    // and kick off another
    StartVideo();
}

bool TorcPiCamera::Stop(void)
{
    emit WritingStopped();

    // stop camera
    OMX_CONFIG_PORTBOOLEANTYPE capture;
    OMX_INITSTRUCTURE(capture);
    capture.nPortIndex = m_cameraVideoPort;
    capture.bEnabled   = OMX_FALSE;
    m_camera.SetConfig(OMX_IndexConfigPortCapturing, &capture);

    // set state to idle
    m_camera.SetState(OMX_StateIdle);
    m_splitter.SetState(OMX_StateIdle);
    m_videoEncoder.SetState(OMX_StateIdle);
    m_imageEncoder.SetState(OMX_StateIdle);
    m_nullSink.SetState(OMX_StateIdle);

    // disable encoder output port
    m_videoEncoder.EnablePort(OMX_DirOutput, 0, false, OMX_IndexParamVideoInit, false);
    m_videoEncoder.DestroyBuffers(OMX_DirOutput, 0, OMX_IndexParamVideoInit);
    m_videoEncoder.WaitForResponse(OMX_CommandPortDisable, m_videoEncoderOutputPort, 1000);

    // disable image encoder output port
    m_imageEncoder.EnablePort(OMX_DirOutput, 0, false, OMX_IndexParamImageInit, false);
    m_imageEncoder.DestroyBuffers(OMX_DirOutput, 0, OMX_IndexParamImageInit);
    m_imageEncoder.WaitForResponse(OMX_CommandPortDisable, m_imageEncoderOutputPort, 1000);

    // disable remaining ports
    m_camera.DisablePorts(OMX_IndexParamVideoInit);
    m_splitter.DisablePorts(OMX_IndexParamVideoInit);
    m_videoEncoder.DisablePorts(OMX_IndexParamVideoInit);
    m_imageEncoder.DisablePorts(OMX_IndexParamImageInit);
    m_nullSink.DisablePorts(OMX_IndexParamVideoInit);

    // destroy tunnels
    m_videoTunnel.Destroy();
    m_previewTunnel.Destroy();
    m_splitterTunnel.Destroy();
    m_imageTunnel.Destroy();

    // set state to loaded
    m_camera.SetState(OMX_StateLoaded);
    m_splitter.SetState(OMX_StateLoaded);
    m_videoEncoder.SetState(OMX_StateLoaded);
    m_imageEncoder.SetState(OMX_StateLoaded);
    m_nullSink.SetState(OMX_StateLoaded);

    return true;
}

bool TorcPiCamera::LoadDrivers(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Trying to load Broadcom drivers");

    // request a callback
    OMX_CONFIG_REQUESTCALLBACKTYPE callback;
    OMX_INITSTRUCTURE(callback);
    callback.nPortIndex = OMX_ALL;
    callback.nIndex     = OMX_IndexParamCameraDeviceNumber;
    callback.bEnable    = OMX_TRUE;
    if (m_camera.SetConfig(OMX_IndexConfigRequestCallback, &callback))
        return false;

    // and set the camera device number. Once we have a response, the relevant
    // drivers should be loaded.
    OMX_PARAM_U32TYPE device;
    OMX_INITSTRUCTURE(device);
    device.nPortIndex = OMX_ALL;
    device.nU32       = 0;
    if (m_camera.SetConfig(OMX_IndexParamCameraDeviceNumber, &device))
        return false;
    if (m_camera.WaitForResponse(OMX_EventParamOrConfigChanged, 0, 10000))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Broadcom drivers loaded");

    OMX_CONFIG_CAMERAINFOTYPE info;
    OMX_INITSTRUCTURE(info);
    if (!m_camera.GetConfig(OMX_IndexConfigCameraInfo, &info))
    {
        if (qstrcmp((const char*)info.cameraname, "imx219") == 0)
            m_cameraType = V2;
        else if (qstrcmp((const char*)info.cameraname, "ov5647") == 0)
            m_cameraType = V1;
        LOG(VB_GENERAL, LOG_INFO, QString("Camera: name '%1' (Version %2)")
            .arg((const char*)info.cameraname)
            .arg(m_cameraType == V2 ? "2" : m_cameraType == V1 ? "1" : "Unknown"));
    }

    return true;
}

bool TorcPiCamera::LoadCameraSettings(void)
{
    if (!m_cameraVideoPort)
        return false;

    // Sharpness
    OMX_CONFIG_SHARPNESSTYPE sharpness;
    OMX_INITSTRUCTURE(sharpness);
    sharpness.nPortIndex = OMX_ALL;
    sharpness.nSharpness = CAMERA_SHARPNESS;
    if (m_camera.SetConfig(OMX_IndexConfigCommonSharpness, &sharpness))
        return false;

    // Contrast
    OMX_CONFIG_CONTRASTTYPE contrast;
    OMX_INITSTRUCTURE(contrast);
    contrast.nPortIndex = OMX_ALL;
    contrast.nContrast  = CAMERA_CONTRAST;
    if (m_camera.SetConfig(OMX_IndexConfigCommonContrast, &contrast))
        return false;

    // Saturation
    OMX_CONFIG_SATURATIONTYPE saturation;
    OMX_INITSTRUCTURE(saturation);
    saturation.nPortIndex  = OMX_ALL;
    saturation.nSaturation = CAMERA_SATURATION;
    if (m_camera.SetConfig(OMX_IndexConfigCommonSaturation, &saturation))
        return false;

    // Brightness
    OMX_CONFIG_BRIGHTNESSTYPE brightness;
    OMX_INITSTRUCTURE(brightness);
    brightness.nPortIndex  = OMX_ALL;
    brightness.nBrightness = CAMERA_BRIGHTNESS;
    if (m_camera.SetConfig(OMX_IndexConfigCommonBrightness, &brightness))
        return false;

    // Exposure
    OMX_CONFIG_EXPOSUREVALUETYPE exposure;
    OMX_INITSTRUCTURE(exposure);
    exposure.nPortIndex        = OMX_ALL;
    exposure.eMetering         = CAMERA_METERING;
    exposure.xEVCompensation   = (OMX_S32)((CAMERA_EXPOSURE_COMPENSATION << 16)/6.0);
    exposure.nShutterSpeedMsec = (OMX_U32)((CAMERA_SHUTTER_SPEED)*1e6);
    exposure.bAutoShutterSpeed = CAMERA_SHUTTER_SPEED_AUTO;
    exposure.nSensitivity      = CAMERA_ISO;
    exposure.bAutoSensitivity  = CAMERA_ISO_AUTO;
    if (m_camera.SetConfig(OMX_IndexConfigCommonExposureValue, &exposure))
        return false;

    // Exposure control
    OMX_CONFIG_EXPOSURECONTROLTYPE commonexposure;
    OMX_INITSTRUCTURE(commonexposure);
    commonexposure.nPortIndex       = OMX_ALL;
    commonexposure.eExposureControl = CAMERA_EXPOSURE;
    if (m_camera.SetConfig(OMX_IndexConfigCommonExposure, &commonexposure))
        return false;

    // Frame stabilisation
    OMX_CONFIG_FRAMESTABTYPE framestabilisation;
    OMX_INITSTRUCTURE(framestabilisation);
    framestabilisation.nPortIndex = OMX_ALL;
    framestabilisation.bStab      = CAMERA_FRAME_STABILIZATION;
    if (m_camera.SetConfig(OMX_IndexConfigCommonFrameStabilisation, &framestabilisation))
        return false;

    // White balance
    OMX_CONFIG_WHITEBALCONTROLTYPE whitebalance;
    OMX_INITSTRUCTURE(whitebalance);
    whitebalance.nPortIndex       = OMX_ALL;
    whitebalance.eWhiteBalControl = CAMERA_WHITE_BALANCE;
    if (m_camera.SetConfig(OMX_IndexConfigCommonWhiteBalance, &whitebalance))
        return false;

    // White balance gain
    if (!CAMERA_WHITE_BALANCE)
    {
      OMX_CONFIG_CUSTOMAWBGAINSTYPE whitebalancegain;
      OMX_INITSTRUCTURE(whitebalancegain);
      whitebalancegain.xGainR = (CAMERA_WHITE_BALANCE_RED_GAIN << 16) / 1000;
      whitebalancegain.xGainB = (CAMERA_WHITE_BALANCE_BLUE_GAIN << 16) / 1000;
      if (m_camera.SetConfig(OMX_IndexConfigCustomAwbGains, &whitebalancegain))
          return false;
    }

    // Image filter
    OMX_CONFIG_IMAGEFILTERTYPE imagefilter;
    OMX_INITSTRUCTURE(imagefilter);
    imagefilter.nPortIndex   = OMX_ALL;
    imagefilter.eImageFilter = CAMERA_IMAGE_FILTER;
    if (m_camera.SetConfig(OMX_IndexConfigCommonImageFilter, &imagefilter))
        return false;

    // Mirror
    OMX_CONFIG_MIRRORTYPE mirror;
    OMX_INITSTRUCTURE(mirror);
    mirror.nPortIndex = m_cameraVideoPort;
    mirror.eMirror    = CAMERA_MIRROR;
    if (m_camera.SetConfig(OMX_IndexConfigCommonMirror, &mirror))
        return false;

    // Rotation
    OMX_CONFIG_ROTATIONTYPE rotation;
    OMX_INITSTRUCTURE(rotation);
    rotation.nPortIndex = m_cameraVideoPort;
    rotation.nRotation  = CAMERA_ROTATION;
    if (m_camera.SetConfig(OMX_IndexConfigCommonRotate, &rotation))
        return false;

    // Color enhancement
    OMX_CONFIG_COLORENHANCEMENTTYPE color_enhancement;
    OMX_INITSTRUCTURE(color_enhancement);
    color_enhancement.nPortIndex        = OMX_ALL;
    color_enhancement.bColorEnhancement = CAMERA_COLOR_ENABLE;
    color_enhancement.nCustomizedU      = CAMERA_COLOR_U;
    color_enhancement.nCustomizedV      = CAMERA_COLOR_V;
    if (m_camera.SetConfig(OMX_IndexConfigCommonColorEnhancement, &color_enhancement))
        return false;

    // Denoise
    OMX_CONFIG_BOOLEANTYPE denoise;
    OMX_INITSTRUCTURE(denoise);
    denoise.bEnabled = CAMERA_NOISE_REDUCTION;
    if (m_camera.SetConfig(OMX_IndexConfigStillColourDenoiseEnable, &denoise))
        return false;

    //ROI
    OMX_CONFIG_INPUTCROPTYPE roi;
    OMX_INITSTRUCTURE(roi);
    roi.nPortIndex = OMX_ALL;
    roi.xLeft      = (CAMERA_ROI_LEFT << 16) / 100;
    roi.xTop       = (CAMERA_ROI_TOP << 16) / 100;
    roi.xWidth     = (CAMERA_ROI_WIDTH << 16) / 100;
    roi.xHeight    = (CAMERA_ROI_HEIGHT << 16) / 100;
    if (m_camera.SetConfig(OMX_IndexConfigInputCropPercentages, &roi))
        return false;

    //DRC
    OMX_CONFIG_DYNAMICRANGEEXPANSIONTYPE drc;
    OMX_INITSTRUCTURE(drc);
    drc.eMode = CAMERA_DRC;
    if (m_camera.SetConfig(OMX_IndexConfigDynamicRangeExpansion, &drc))
        return false;

    return true;
}

bool TorcPiCamera::ConfigureCamera(void)
{
    if (!m_cameraPreviewPort || !m_cameraVideoPort)
        return false;

    // configure camera video port
    OMX_PARAM_PORTDEFINITIONTYPE videoport;
    OMX_INITSTRUCTURE(videoport);
    videoport.nPortIndex = m_cameraVideoPort;
    if (m_camera.GetParameter(OMX_IndexParamPortDefinition, &videoport))
        return false;

    videoport.format.video.nFrameWidth        = m_params.m_width;
    videoport.format.video.nFrameHeight       = m_params.m_height;
    videoport.format.video.nStride            = m_params.m_stride;
    videoport.format.video.nSliceHeight       = m_params.m_sliceHeight;
    videoport.format.video.xFramerate         = m_params.m_frameRate << 16;
    videoport.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    videoport.format.video.eColorFormat       = OMX_COLOR_FormatYUV420PackedPlanar;
    if (m_camera.SetParameter(OMX_IndexParamPortDefinition, &videoport))
        return false;

    // and the same for the preview port
    videoport.nPortIndex = m_cameraPreviewPort;
    if (m_camera.SetParameter(OMX_IndexParamPortDefinition, &videoport))
        return false;

    // configure video frame rate
    OMX_CONFIG_FRAMERATETYPE framerate;
    OMX_INITSTRUCTURE(framerate);
    framerate.nPortIndex = m_cameraVideoPort;
    framerate.xEncodeFramerate = videoport.format.video.xFramerate;
    if (m_camera.SetConfig(OMX_IndexConfigVideoFramerate, &framerate))
        return false;

    // and same again for preview port
    framerate.nPortIndex = m_cameraPreviewPort;
    if (m_camera.SetConfig(OMX_IndexConfigVideoFramerate, &framerate))
        return false;

    // camera settings
    if (!LoadCameraSettings())
        return false;

    return true;
}

bool TorcPiCamera::ConfigureImageEncoder(void)
{
    if (!m_imageEncoderOutputPort)
        return false;

    OMX_PARAM_PORTDEFINITIONTYPE encoderport;
    OMX_INITSTRUCTURE(encoderport);
    encoderport.nPortIndex = m_imageEncoderOutputPort;
    if (m_imageEncoder.GetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    encoderport.format.image.nFrameWidth        = m_params.m_width;
    encoderport.format.image.nFrameHeight       = m_params.m_height;
    encoderport.format.image.nStride            = m_params.m_stride;
    encoderport.format.image.nSliceHeight       = m_params.m_sliceHeight;
    encoderport.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    encoderport.format.image.eColorFormat       = OMX_COLOR_FormatUnused;

    if (m_imageEncoder.SetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set image encoder output parameters");

    OMX_INITSTRUCTURE(encoderport);
    encoderport.nPortIndex = m_imageEncoder.GetPort(OMX_DirInput, 0, OMX_IndexParamImageInit);
    if (m_imageEncoder.GetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    encoderport.format.image.nFrameWidth        = m_params.m_width;
    encoderport.format.image.nFrameHeight       = m_params.m_height;
    encoderport.format.image.nStride            = m_params.m_stride;
    encoderport.format.image.nSliceHeight       = m_params.m_sliceHeight;
    encoderport.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    encoderport.format.image.eColorFormat       = OMX_COLOR_FormatYUV420PackedPlanar;

    if (m_imageEncoder.SetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set image encoder input parameters");
    return true;
}

bool TorcPiCamera::ConfigureVideoEncoder(void)
{
    if (!m_videoEncoderOutputPort)
        return false;

    // Encoder output
    OMX_PARAM_PORTDEFINITIONTYPE encoderport;
    OMX_INITSTRUCTURE(encoderport);
    encoderport.nPortIndex = m_videoEncoderOutputPort;
    if (m_videoEncoder.GetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    encoderport.format.video.nFrameWidth        = m_params.m_width;
    encoderport.format.video.nFrameHeight       = m_params.m_height;
    encoderport.format.video.nStride            = m_params.m_stride;
    encoderport.format.video.nSliceHeight       = m_params.m_sliceHeight;
    encoderport.format.video.xFramerate         = m_params.m_frameRate << 16;
    encoderport.format.video.nBitrate           = ENCODER_QP ? 0 : m_params.m_bitrate;
    encoderport.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    if (m_videoEncoder.SetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder output parameters");

    if (!ENCODER_QP)
    {
        OMX_VIDEO_PARAM_BITRATETYPE bitrate;
        OMX_INITSTRUCTURE(bitrate);
        bitrate.eControlRate   = OMX_Video_ControlRateVariable;
        bitrate.nTargetBitrate = m_params.m_bitrate;
        bitrate.nPortIndex     = m_videoEncoderOutputPort;
        if (m_videoEncoder.SetParameter(OMX_IndexParamVideoBitrate, &bitrate))
            return false;

        LOG(VB_GENERAL, LOG_INFO, "Set video encoder output bitrate");
    }
    else
    {
        // quantization
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;
        OMX_INITSTRUCTURE(quantization);
        quantization.nPortIndex = m_videoEncoderOutputPort;
        quantization.nQpI       = ENCODER_QP_I;
        quantization.nQpP       = ENCODER_QP_P;
        if (m_videoEncoder.SetParameter(OMX_IndexParamVideoQuantization, &quantization))
            return false;

        LOG(VB_GENERAL, LOG_INFO, "Set video encoder output quantization");
    }

    // codec
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    OMX_INITSTRUCTURE(format);
    format.nPortIndex         = m_videoEncoderOutputPort;
    format.eCompressionFormat = OMX_VIDEO_CodingAVC;
    if (m_videoEncoder.SetParameter(OMX_IndexParamVideoPortFormat, &format))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder output format");

    // IDR period
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD idr;
    OMX_INITSTRUCTURE(idr);
    idr.nPortIndex = m_videoEncoderOutputPort;
    if (m_videoEncoder.GetConfig(OMX_IndexConfigVideoAVCIntraPeriod, &idr))
        return false;
    idr.nIDRPeriod = m_params.m_frameRate * VIDEO_GOPDURA_TARGET;
    if (m_videoEncoder.SetConfig(OMX_IndexConfigVideoAVCIntraPeriod, &idr))
        return false;

    LOG(VB_GENERAL, LOG_INFO, QString("Set video encoder output IDR to %1").arg(m_params.m_frameRate * VIDEO_GOPDURA_TARGET));

    // SEI
    OMX_PARAM_BRCMVIDEOAVCSEIENABLETYPE sei;
    OMX_INITSTRUCTURE(sei);
    sei.nPortIndex = m_videoEncoderOutputPort;
    sei.bEnable    = ENCODER_SEI;
    if (m_videoEncoder.SetParameter(OMX_IndexParamBrcmVideoAVCSEIEnable, &sei))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder output SEI");

    // EEDE
    OMX_VIDEO_EEDE_ENABLE eede;
    OMX_INITSTRUCTURE(eede);
    eede.nPortIndex = m_videoEncoderOutputPort;
    eede.enable     = ENCODER_EEDE;
    if (m_videoEncoder.SetParameter(OMX_IndexParamBrcmEEDEEnable, &eede))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video endcoder output EEDE");

    OMX_VIDEO_EEDE_LOSSRATE eede_loss_rate;
    OMX_INITSTRUCTURE(eede_loss_rate);
    eede_loss_rate.nPortIndex = m_videoEncoderOutputPort;
    eede_loss_rate.loss_rate  = ENCODER_EEDE_LOSS_RATE;
    if (m_videoEncoder.SetParameter(OMX_IndexParamBrcmEEDELossRate, &eede_loss_rate))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder output EEDE loss rate");

    // profile
    OMX_VIDEO_AVCPROFILETYPE profile = OMX_VIDEO_AVCProfileMain;
    switch (VIDEO_H264_PROFILE)
    {
        case FF_PROFILE_H264_BASELINE: profile = OMX_VIDEO_AVCProfileBaseline; break;
        case FF_PROFILE_H264_MAIN:     profile = OMX_VIDEO_AVCProfileMain;     break;
        case FF_PROFILE_H264_EXTENDED: profile = OMX_VIDEO_AVCProfileExtended; break;
        case FF_PROFILE_H264_HIGH:     profile = OMX_VIDEO_AVCProfileHigh;     break;
        case FF_PROFILE_H264_HIGH_10:  profile = OMX_VIDEO_AVCProfileHigh10;   break;
        case FF_PROFILE_H264_HIGH_422: profile = OMX_VIDEO_AVCProfileHigh422;  break;
        case FF_PROFILE_H264_HIGH_444: profile = OMX_VIDEO_AVCProfileHigh444;  break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, "Unknown H264 profile. Defaulting to main");
    }

    OMX_VIDEO_PARAM_AVCTYPE avc;
    OMX_INITSTRUCTURE(avc);
    avc.nPortIndex = m_videoEncoderOutputPort;
    if (m_videoEncoder.GetParameter(OMX_IndexParamVideoAvc, &avc))
        return false;
    avc.eProfile = profile;
    if (m_videoEncoder.SetParameter(OMX_IndexParamVideoAvc, &avc))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder output profile");

    // Inline SPS/PPS
    OMX_CONFIG_PORTBOOLEANTYPE headers;
    OMX_INITSTRUCTURE(headers);
    headers.nPortIndex = m_videoEncoderOutputPort;
    headers.bEnabled   = ENCODER_INLINE_HEADERS;
    if (m_videoEncoder.SetParameter(OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &headers))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder output SPS/PPS");

    // SPS timing
    OMX_CONFIG_PORTBOOLEANTYPE timing;
    OMX_INITSTRUCTURE(timing);
    timing.nPortIndex =  m_videoEncoderOutputPort;
    timing.bEnabled   = ENCODER_SPS_TIMING;
    if (m_videoEncoder.SetParameter(OMX_IndexParamBrcmVideoAVCSPSTimingEnable, &timing))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set video encoder SPS timings");
    return true;
}

bool TorcPiCamera::EnableStills(uint Count)
{
    OMX_PARAM_U32TYPE single;
    OMX_INITSTRUCTURE(single);
    single.nPortIndex = m_splitter.GetPort(OMX_DirOutput, 1, OMX_IndexParamVideoInit);
    single.nU32       = Count;
    if (m_splitter.SetParameter(OMX_IndexConfigSingleStep, &single))
        return false;

    return TorcCameraDevice::EnableStills(Count);
}

static const QString piCameraType =
"    <xs:element name=\"pi\" type=\"cameraType\"/>\r\n";

class TorcPiCameraXSDFactory : public TorcXSDFactory
{
  public:
    void GetXSD(QMultiMap<QString,QString> &XSD) { XSD.insert(XSD_CAMERATYPES, piCameraType); }
} TorcPiCameraXSDFactory;

class TorcPiCameraFactory final : public TorcCameraFactory
{
  public:
    TorcPiCameraFactory() : TorcCameraFactory()
    {
        bcm_host_init();
    }

   virtual ~TorcPiCameraFactory()
    {
        bcm_host_deinit();
    }

    bool CanHandle(const QString &Type, const TorcCameraParams &Params) override
    {
        (void)Params;

        static bool checked = false;
        if (!checked)
        {
            checked = true;

            char command[32];
            int  gpumem = 0;

            if (!vc_gencmd(command, sizeof(command), "get_mem gpu"))
                vc_gencmd_number_property(command, "gpu", &gpumem);

            if (gpumem < 128)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Insufficent GPU memory for Pi camera - need 128Mb - have %1Mb").arg(gpumem));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, QString("%1Mb GPU memory").arg(gpumem));

                int supported = 0;
                int detected  = 0;

                if (!vc_gencmd(command, sizeof(command), "get_camera"))
                {
                    vc_gencmd_number_property(command, "supported", &supported);
                    vc_gencmd_number_property(command, "detected",  &detected);
                }

                if (!supported)
                {
                    LOG(VB_GENERAL, LOG_ERR, "Firmware reports that Pi camera is NOT supported");
                }
                else if (!detected)
                {
                    LOG(VB_GENERAL, LOG_ERR, "Firmware reports that Pi camera NOT detected");
                }
                else
                {
                    LOG(VB_GENERAL, LOG_INFO, "Pi camera supported and detected");
                    TorcPiCamera::gPiCameraDetected = true;
                }
            }
        }

        return TorcPiCamera::gPiCameraDetected ? "pi" == Type : false;
    }

    TorcCameraDevice* Create(const QString &Type, const TorcCameraParams &Params) override
    {
        if (("pi" == Type) && TorcPiCamera::gPiCameraDetected)
            return new TorcPiCamera(Params);
        return NULL;
    }
} TorcPiCameraFactory;

/* vim: set expandtab tabstop=4 shiftwidth=4: */
