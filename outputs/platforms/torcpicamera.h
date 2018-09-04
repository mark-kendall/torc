#ifndef TORCPICAMERA_H
#define TORCPICAMERA_H

// Torc
#include "torcomxcore.h"
#include "torcomxport.h"
#include "torcomxcomponent.h"
#include "torcomxtunnel.h"
#include "ffmpeg/torcmuxer.h"
#include "torccamera.h"

// FFmpeg
#include <libavcodec/avcodec.h>

class TorcPiCamera Q_DECL_FINAL : public TorcCameraDevice 
{
    Q_OBJECT

  public:
    enum CameraType
    {
        Unknown = 0,
        V1      = 1,
        V2      = 2
    };

    static bool             gPiCameraDetected;
    TorcPiCamera(const TorcCameraParams &Params);
    virtual ~TorcPiCamera();

    bool Setup              (void) Q_DECL_OVERRIDE;
    bool WriteFrame         (void) Q_DECL_OVERRIDE;
    bool Stop               (void) Q_DECL_OVERRIDE;

  private:
    bool LoadDrivers        (void);
    bool LoadCameraSettings (void);
    bool ConfigureCamera    (void);
    bool ConfigureEncoder   (void);

  private:
    Q_DISABLE_COPY(TorcPiCamera)

    CameraType               m_cameraType;
    TorcOMXCore              m_core;
    TorcOMXComponent         m_camera;
    TorcOMXComponent         m_encoder;
    TorcOMXComponent         m_nullSink;
    OMX_U32                  m_cameraPreviewPort;
    OMX_U32                  m_cameraVideoPort;
    OMX_U32                  m_encoderInputPort;
    OMX_U32                  m_encoderOutputPort;
    TorcOMXTunnel            m_videoTunnel;
    TorcOMXTunnel            m_previewTunnel;

    TorcMuxer               *m_muxer;
    int                      m_videoStream;
    quint64                  m_frameCount;
    AVPacket                *m_bufferedPacket;
    bool                     m_haveInitSegment;
};

#endif // TORCPICAMERA_H
