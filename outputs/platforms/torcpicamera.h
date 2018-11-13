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

class TorcPiCamera final : public TorcCameraDevice 
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

    bool Setup              (void) override;
    bool WriteFrame         (void) override;
    bool Stop               (void) override;

  private:
    bool LoadDrivers        (void);
    bool LoadCameraSettings (void);
    bool ConfigureCamera    (void);
    bool ConfigureVideoEncoder(void);
    bool ConfigureImageEncoder(void);
    void ClearSnapshotBuffers(void);

  private:
    Q_DISABLE_COPY(TorcPiCamera)

    CameraType               m_cameraType;
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

    TorcMuxer               *m_muxer;
    int                      m_videoStream;
    quint64                  m_frameCount;
    AVPacket                *m_bufferedPacket;
    bool                     m_haveInitSegment;
    bool                     m_takeSnapshot;
    QList<QPair<quint32, uint8_t*> > m_snapshotBuffers;
};

#endif // TORCPICAMERA_H
