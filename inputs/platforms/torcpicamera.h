#ifndef TORCPICAMERA_H
#define TORCPICAMERA_H

// Torc
#include "torcomxcore.h"
#include "torcomxport.h"
#include "torcomxcomponent.h"
#include "torcomxtunnel.h"

class TorcPiCamera
{
  public:
    TorcPiCamera();
   ~TorcPiCamera();

    bool Start              (void);
    void Stop               (void);

  private:
    bool LoadDrivers        (void);
    bool LoadCameraSettings (void);
    bool ConfigureCamera    (void);
    bool ConfigureEncoder   (void);

  private:
    Q_DISABLE_COPY(TorcPiCamera)

    TorcOMXCore             *m_core;
    TorcOMXComponent        *m_camera;
    TorcOMXComponent        *m_encoder;
    TorcOMXComponent        *m_nullSink;
    OMX_U32                  m_cameraPreviewPort;
    OMX_U32                  m_cameraVideoPort;
    OMX_U32                  m_encoderInputPort;
    OMX_U32                  m_encoderOutputPort;
    TorcOMXTunnel           *m_videoTunnel;
    TorcOMXTunnel           *m_previewTunnel;
};

#endif // TORCPICAMERA_H
