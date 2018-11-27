#ifndef TORCPICAMERA_H
#define TORCPICAMERA_H

// Torc
#include "torcomxcore.h"
#include "torcomxport.h"
#include "torcomxcomponent.h"
#include "torcomxtunnel.h"
#include "torccamera.h"

class TorcPiCamera final : public TorcCameraDevice 
{
    Q_OBJECT

  public:
    enum PiCameraType
    {
        Unknown = 0,
        V1      = 1,
        V2      = 2
    };

    static bool             gPiCameraDetected;
    TorcPiCamera(const TorcCameraParams &Params);
    virtual ~TorcPiCamera();

    bool Setup              (void) override;
    bool Start              (void) override;
    bool Stop               (void) override;

  public slots:
    void StreamVideo        (bool Video) override;
    void BufferReady        (OMX_BUFFERHEADERTYPE *Buffer, quint64 Type);

  protected:
    bool EnableStills       (uint Count) override;
    void StartStill         (void) override;

  private:
    bool LoadDrivers        (void);
    bool LoadCameraSettings (void);
    bool ConfigureCamera    (void);
    bool ConfigureVideoEncoder(void);
    bool ConfigureImageEncoder(void);

    void StartVideo         (void);
    void ProcessVideoBuffer (OMX_BUFFERHEADERTYPE *Buffer);
    bool EnableVideo        (bool Video);
    void ProcessStillsBuffer(OMX_BUFFERHEADERTYPE *Buffer);


  private:
    Q_DISABLE_COPY(TorcPiCamera)

    PiCameraType             m_cameraType;

    // OpenMax
    TorcOMXCore              m_core;
    TorcOMXComponent         m_camera;
    TorcOMXComponent         m_videoEncoder;
    TorcOMXComponent         m_nullSink;
    TorcOMXComponent         m_splitter;
    TorcOMXComponent         m_imageEncoder;
    OMX_U32                  m_cameraPreviewPort;
    OMX_U32                  m_cameraVideoPort;
    OMX_U32                  m_videoEncoderInputPort;
    OMX_U32                  m_videoEncoderOutputPort;
    OMX_U32                  m_imageEncoderOutputPort;
    TorcOMXTunnel            m_splitterTunnel;
    TorcOMXTunnel            m_videoTunnel;
    TorcOMXTunnel            m_previewTunnel;
    TorcOMXTunnel            m_imageTunnel;
};

#endif // TORCPICAMERA_H
