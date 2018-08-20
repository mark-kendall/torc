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
#include <QFile>
#include <QTime>

// Torc
#include "torclogging.h"
#include "torccentral.h"
#include "torcpicamera.h"

// OpenMax
#include "OMX_Video.h"

// Broadcom
#include "OMX_Broadcom.h"

#define BROADCOM_CAMERA                "OMX.broadcom.camera"
#define BROADCOM_ENCODER               "OMX.broadcom.video_encode"
#define BROADCOM_NULLSINK              "OMX.broadcom.null_sink"
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
#define ENCODER_BITRATE                4000000
#define ENCODER_QP                     OMX_FALSE
#define ENCODER_QP_I                   0
#define ENCODER_QP_P                   0
#define ENCODER_IDR_PERIOD             30 // once every second
#define ENCODER_SEI                    OMX_FALSE
#define ENCODER_EEDE                   OMX_FALSE
#define ENCODER_EEDE_LOSS_RATE         0
#define ENCODER_PROFILE                OMX_VIDEO_AVCProfileMain
#define ENCODER_INLINE_HEADERS         OMX_TRUE
#define ENCODER_SPS_TIMING             OMX_TRUE
#define VIDEO_WIDTH                    1280
#define VIDEO_HEIGHT                   720
#define VIDEO_FRAMERATE                30

TorcPiCamera::TorcPiCamera(const TorcCameraParams &Params)
  : TorcCameraDevice(Params),
    m_core(),
    m_camera((char*)BROADCOM_CAMERA),
    m_encoder((char*)BROADCOM_ENCODER),
    m_nullSink((char*)BROADCOM_NULLSINK),
    m_cameraPreviewPort(0),
    m_cameraVideoPort(0),
    m_encoderInputPort(0),
    m_encoderOutputPort(0),
    m_videoTunnel(&m_camera, 1, OMX_IndexParamVideoInit, &m_encoder,  0, OMX_IndexParamVideoInit),
    m_previewTunnel(&m_camera, 0, OMX_IndexParamVideoInit, &m_nullSink, 0, OMX_IndexParamVideoInit),
    m_muxer(NULL),
    m_videoStream(0),
    m_frameCount(0),
    m_bufferedPacket(NULL),
    m_haveInitSegment(false)
{
}

TorcPiCamera::~TorcPiCamera()
{
    Stop();
}

bool TorcPiCamera::Setup(void)
{
    // NB Pi hardware is initialised in TorcPiGPIO as it is static

    if (!m_camera.IsValid() || !m_encoder.IsValid() || !m_nullSink.IsValid())
        return false;

    // load drivers - Broadcom specific
    if (!LoadDrivers())
        return false;

    // retrieve port numbers
    m_cameraPreviewPort = m_camera.GetPort(OMX_DirOutput,  0, OMX_IndexParamVideoInit);
    m_cameraVideoPort   = m_camera.GetPort(OMX_DirOutput,  1, OMX_IndexParamVideoInit);
    m_encoderInputPort  = m_encoder.GetPort(OMX_DirInput,  0, OMX_IndexParamVideoInit);
    m_encoderOutputPort = m_encoder.GetPort(OMX_DirOutput, 0, OMX_IndexParamVideoInit);

    if (!m_cameraVideoPort || !m_cameraPreviewPort || !m_encoderInputPort || !m_encoderOutputPort)
        return false;

    // configure camera
    if (!ConfigureCamera())
        return false;

    // configure encoder
    if (!ConfigureEncoder())
        return false;

    // create tunnels
    if (m_videoTunnel.Create() || m_previewTunnel.Create())
        return false;

    // set to idle
    m_camera.SetState(OMX_StateIdle);
    m_encoder.SetState(OMX_StateIdle);
    m_nullSink.SetState(OMX_StateIdle);

    // TODO add error checking to these calls
    // enable ports
    m_camera.EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit);
    m_camera.EnablePort(OMX_DirOutput, 1, true, OMX_IndexParamVideoInit);
    m_encoder.EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);
    m_encoder.EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit, false);
    m_encoder.CreateBuffers(OMX_DirOutput, 0, OMX_IndexParamVideoInit);
    m_encoder.WaitForResponse(OMX_CommandPortEnable, m_encoderOutputPort, 1000);
    m_nullSink.EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);

    // set state to execute
    m_camera.SetState(OMX_StateExecuting);
    m_encoder.SetState(OMX_StateExecuting);
    m_nullSink.SetState(OMX_StateExecuting);

    // enable
    OMX_CONFIG_PORTBOOLEANTYPE capture;
    OMX_INITSTRUCTURE(capture);
    capture.nPortIndex = m_cameraVideoPort;
    capture.bEnabled   = OMX_TRUE;
    if (m_camera.SetConfig(OMX_IndexConfigPortCapturing, &capture))
        return false;

    int profile = FF_PROFILE_H264_BASELINE;
    switch (ENCODER_PROFILE)
    {
        case OMX_VIDEO_AVCProfileBaseline: profile = FF_PROFILE_H264_BASELINE; break;
        case OMX_VIDEO_AVCProfileMain:     profile = FF_PROFILE_H264_MAIN;     break;
        case OMX_VIDEO_AVCProfileExtended: profile = FF_PROFILE_H264_EXTENDED; break;
        case OMX_VIDEO_AVCProfileHigh:     profile = FF_PROFILE_H264_HIGH;     break;
        case OMX_VIDEO_AVCProfileHigh10:   profile = FF_PROFILE_H264_HIGH_10;  break;
        case OMX_VIDEO_AVCProfileHigh422:  profile = FF_PROFILE_H264_HIGH_422; break;
        case OMX_VIDEO_AVCProfileHigh444:  profile = FF_PROFILE_H264_HIGH_444; break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, "Unknown H264 profile. Defaulting to baseline");
    }

    int buffersize = (m_params.m_bitrate * (m_params.m_segmentLength / (m_params.m_frameRateNum / m_params.m_frameRateDiv)) * VIDEO_SEGMENT_NUMBER) / 8;
    m_ringBuffer   = new TorcSegmentedRingBuffer(buffersize);    
    m_muxer        = new TorcMuxer(m_ringBuffer);
    if (!m_muxer)
        return false;

    m_videoStream = m_muxer->AddH264Stream(VIDEO_WIDTH, VIDEO_HEIGHT, profile, ENCODER_BITRATE);
    if (!m_muxer->IsValid())
        return false;

    connect(m_ringBuffer, SIGNAL(SegmentReady(int)),    this, SIGNAL(SegmentReady(int)));
    connect(m_ringBuffer, SIGNAL(SegmentRemoved(int)),  this, SIGNAL(SegmentRemoved(int)));

    m_frameCount      = 0;
    m_bufferedPacket  = NULL;
    m_haveInitSegment = false;

    LOG(VB_GENERAL, LOG_INFO, "Pi camera setup");
    return true;
}

bool TorcPiCamera::WriteFrame(void)
{
    if (!m_muxer)
        return false;

    OMX_BUFFERHEADERTYPE* buffer = m_encoder.GetBuffer(OMX_DirOutput, 0, 1000, OMX_IndexParamVideoInit);
    if (!buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve OMX buffer");
        return false;
    }

    if (m_encoder.FillThisBuffer(buffer))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to fill OMX buffer");
        return false;
    }

    while (m_encoder.GetAvailableBuffers(OMX_DirOutput, 0, OMX_IndexParamVideoInit) < 1)
        QThread::usleep(500);

    bool idr      = buffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME;
    bool complete = buffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME;
    bool sps      = buffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG;

    if (!sps && complete)
        m_frameCount++;

    // TODO wrap pts
    // this increases the pts monotonically based on actual complete frames received
    qint64 pts = m_frameCount * (90000.0 / (float)VIDEO_FRAMERATE);
    // sps is not considered a frame but muxer will complain if the pts does not increase
    if (sps)
        pts++;

    // NB we use av_packet_from_data and av_grow_packet for the buffered packet - which uses the internal AVBufferRef structure
    // Do not manually release AVPacket->data
    if (m_bufferedPacket)
    {
        quint32 offset = m_bufferedPacket->size;
        av_grow_packet(m_bufferedPacket, buffer->nFilledLen);
        memcpy(m_bufferedPacket->data + offset, reinterpret_cast<uint8_t*>(buffer->pBuffer) + buffer->nOffset, buffer->nFilledLen);
    }

    if (complete)
    {
        AVPacket *packet = m_bufferedPacket;
        if (!packet)
        {
            packet = av_packet_alloc();
            av_init_packet(packet);
            packet->size = buffer->nFilledLen;
            packet->data = reinterpret_cast<uint8_t*>(buffer->pBuffer) + buffer->nOffset;
        }

        packet->stream_index = m_videoStream;
        packet->pts          = pts;
        packet->dts          = pts;
        packet->duration     = 1;
        packet->flags       |= idr ? AV_PKT_FLAG_KEY : 0;
        m_muxer->AddPacket(packet, sps);

        if (sps && !m_haveInitSegment)
        {
            m_haveInitSegment = true;
            m_muxer->FinishSegment(true);
        }
        else if (!sps && !(m_frameCount % (VIDEO_FRAMERATE * VIDEO_SEGMENT_TARGET)))
        {
            m_muxer->FinishSegment(false);
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
    else
    {
        if (!m_bufferedPacket)
        {
            uint8_t* data = (uint8_t*)av_malloc(buffer->nFilledLen);
            m_bufferedPacket = av_packet_alloc();
            av_init_packet(m_bufferedPacket);
            av_packet_from_data(m_bufferedPacket, data, buffer->nFilledLen);
            memcpy(m_bufferedPacket->data, reinterpret_cast<uint8_t*>(buffer->pBuffer) + buffer->nOffset, buffer->nFilledLen);
            m_bufferedPacket->flags |= idr ? AV_PKT_FLAG_KEY : 0;
        }
    }

    return true;
}

bool TorcPiCamera::Stop(void)
{
    if (m_bufferedPacket)
    {
        av_packet_free(&m_bufferedPacket);
        m_bufferedPacket = NULL;
    }

    if (m_muxer)
    {
        m_muxer->Finish();
        delete m_muxer;
        m_muxer = NULL;
    }

    if (m_ringBuffer)
    {
        delete m_ringBuffer;
        m_ringBuffer = NULL;
    }

    m_frameCount      = 0;
    m_videoStream     = 0;
    m_haveInitSegment = false;

    // stop camera
    OMX_CONFIG_PORTBOOLEANTYPE capture;
    OMX_INITSTRUCTURE(capture);
    capture.nPortIndex = m_cameraVideoPort;
    capture.bEnabled   = OMX_FALSE;
    m_camera.SetConfig(OMX_IndexConfigPortCapturing, &capture);

    // set state to idle
    m_camera.SetState(OMX_StateIdle);
    m_encoder.SetState(OMX_StateIdle);
    m_nullSink.SetState(OMX_StateIdle);

    // disable encoder output port
    m_encoder.EnablePort(OMX_DirOutput, 0, false, OMX_IndexParamVideoInit, false);
    m_encoder.DestroyBuffers(OMX_DirOutput, 0, OMX_IndexParamVideoInit);
    m_encoder.WaitForResponse(OMX_CommandPortDisable, m_encoderOutputPort, 1000);

    // disable remaining ports
    m_camera.DisablePorts(OMX_IndexParamVideoInit);
    m_encoder.DisablePorts(OMX_IndexParamVideoInit);
    m_nullSink.DisablePorts(OMX_IndexParamVideoInit);

    // destroy tunnels
    m_videoTunnel.Destroy();
    m_previewTunnel.Destroy();

    // set state to loaded
    m_camera.SetState(OMX_StateLoaded);
    m_encoder.SetState(OMX_StateLoaded);
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

    videoport.format.video.nFrameWidth        = VIDEO_WIDTH;
    videoport.format.video.nFrameHeight       = VIDEO_HEIGHT;
    videoport.format.video.nStride            = VIDEO_WIDTH;
    videoport.format.video.xFramerate         = VIDEO_FRAMERATE << 16;
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

bool TorcPiCamera::ConfigureEncoder(void)
{
    if (!m_encoderOutputPort)
        return false;

    // Encoder output
    OMX_PARAM_PORTDEFINITIONTYPE encoderport;
    OMX_INITSTRUCTURE(encoderport);
    encoderport.nPortIndex = m_encoderOutputPort;
    if (m_encoder.GetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    encoderport.format.video.nFrameWidth        = VIDEO_WIDTH;
    encoderport.format.video.nFrameHeight       = VIDEO_HEIGHT;
    encoderport.format.video.nStride            = VIDEO_WIDTH;
    encoderport.format.video.xFramerate         = VIDEO_FRAMERATE << 16;
    encoderport.format.video.nBitrate           = ENCODER_QP ? 0 : ENCODER_BITRATE;
    encoderport.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    if (m_encoder.SetParameter(OMX_IndexParamPortDefinition, &encoderport))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output parameters");

    if (!ENCODER_QP)
    {
        OMX_VIDEO_PARAM_BITRATETYPE bitrate;
        OMX_INITSTRUCTURE(bitrate);
        bitrate.eControlRate   = OMX_Video_ControlRateVariable;
        bitrate.nTargetBitrate = ENCODER_BITRATE;
        bitrate.nPortIndex     = m_encoderOutputPort;
        if (m_encoder.SetParameter(OMX_IndexParamVideoBitrate, &bitrate))
            return false;

        LOG(VB_GENERAL, LOG_INFO, "Set encoder output bitrate");
    }
    else
    {
        // quantization
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;
        OMX_INITSTRUCTURE(quantization);
        quantization.nPortIndex = m_encoderOutputPort;
        quantization.nQpI       = ENCODER_QP_I;
        quantization.nQpP       = ENCODER_QP_P;
        if (m_encoder.SetParameter(OMX_IndexParamVideoQuantization, &quantization))
            return false;

        LOG(VB_GENERAL, LOG_INFO, "Set encoder output quantization");
    }

    // codec
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    OMX_INITSTRUCTURE(format);
    format.nPortIndex         = m_encoderOutputPort;
    format.eCompressionFormat = OMX_VIDEO_CodingAVC;
    if (m_encoder.SetParameter(OMX_IndexParamVideoPortFormat, &format))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output format");

    // IDR period
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD idr;
    OMX_INITSTRUCTURE(idr);
    idr.nPortIndex = m_encoderOutputPort;
    if (m_encoder.GetConfig(OMX_IndexConfigVideoAVCIntraPeriod, &idr))
        return false;
    idr.nIDRPeriod = ENCODER_IDR_PERIOD;
    if (m_encoder.SetConfig(OMX_IndexConfigVideoAVCIntraPeriod, &idr))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output IDR");

    // SEI
    OMX_PARAM_BRCMVIDEOAVCSEIENABLETYPE sei;
    OMX_INITSTRUCTURE(sei);
    sei.nPortIndex = m_encoderOutputPort;
    sei.bEnable    = ENCODER_SEI;
    if (m_encoder.SetParameter(OMX_IndexParamBrcmVideoAVCSEIEnable, &sei))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output SEI");

    // EEDE
    OMX_VIDEO_EEDE_ENABLE eede;
    OMX_INITSTRUCTURE(eede);
    eede.nPortIndex = m_encoderOutputPort;
    eede.enable     = ENCODER_EEDE;
    if (m_encoder.SetParameter(OMX_IndexParamBrcmEEDEEnable, &eede))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set endcoder output EEDE");

    OMX_VIDEO_EEDE_LOSSRATE eede_loss_rate;
    OMX_INITSTRUCTURE(eede_loss_rate);
    eede_loss_rate.nPortIndex = m_encoderOutputPort;
    eede_loss_rate.loss_rate  = ENCODER_EEDE_LOSS_RATE;
    if (m_encoder.SetParameter(OMX_IndexParamBrcmEEDELossRate, &eede_loss_rate))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output EEDE loss rate");

    // profile
    OMX_VIDEO_PARAM_AVCTYPE avc;
    OMX_INITSTRUCTURE(avc);
    avc.nPortIndex = m_encoderOutputPort;
    if (m_encoder.GetParameter(OMX_IndexParamVideoAvc, &avc))
        return false;
    avc.eProfile = ENCODER_PROFILE;
    if (m_encoder.SetParameter(OMX_IndexParamVideoAvc, &avc))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output profile");

    // Inline SPS/PPS
    OMX_CONFIG_PORTBOOLEANTYPE headers;
    OMX_INITSTRUCTURE(headers);
    headers.nPortIndex = m_encoderOutputPort;
    headers.bEnabled   = ENCODER_INLINE_HEADERS;
    if (m_encoder.SetParameter(OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &headers))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder output SPS/PPS");

    // SPS timing
    OMX_CONFIG_PORTBOOLEANTYPE timing;
    OMX_INITSTRUCTURE(timing);
    timing.nPortIndex =  m_encoderOutputPort;
    timing.bEnabled   = ENCODER_SPS_TIMING;
    if (m_encoder.SetParameter(OMX_IndexParamBrcmVideoAVCSPSTimingEnable, &timing))
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Set encoder SPS timings");
    return true;
}

static const QString piCameraType =
"    <xs:element name=\"pi\" type=\"cameraType\"/>\r\n";

class TorcPiCameraXSDFactory : public TorcXSDFactory
{
  public:
    void GetXSD(QMultiMap<QString,QString> &XSD) { XSD.insert(XSD_CAMERATYPES, piCameraType); }
} TorcPiCameraXSDFactory;

class TorcPiCameraFactory Q_DECL_FINAL : public TorcCameraFactory
{
  public:
    bool CanHandle(const QString &Type, const TorcCameraParams &Params) Q_DECL_OVERRIDE
    {
        (void)Params;
        return "pi" == Type;
    }

    TorcCameraDevice* Create(const QString &Type, const TorcCameraParams &Params) Q_DECL_OVERRIDE
    {
        if ("pi" == Type)
            return new TorcPiCamera(Params);
        return NULL;
    }
} TorcPiCameraFactory;

/* vim: set expandtab tabstop=4 shiftwidth=4: */
