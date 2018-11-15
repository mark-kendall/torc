#ifndef TORCCAMERAOUTPUT_H
#define TORCCAMERAOUTPUT_H

// Torc
#include "torcoutput.h"
#include "torccamera.h"

class TorcCameraThread;
class TorcCameraParams;

class TorcCameraOutput final : public TorcOutput
{
    Q_OBJECT

  public:
    TorcCameraOutput(const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcCameraOutput();

    TorcOutput::Type GetType            (void) override;
    void             Start              (void) override;
    void             Stop               (void) override;
    QString          GetPresentationURL (void) override;
    void             ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress,
                                         int LocalPort, TorcHTTPRequest &Request) override;
  public slots:
    void             WritingStarted     (void);
    void             WritingStopped     (void);
    void             CameraErrored      (bool Errored);
    void             SegmentRemoved     (int Segment);
    void             InitSegmentReady   (void);
    void             SegmentReady       (int Segment);

  private:
    QByteArray       GetMasterPlaylist  (void);
    QByteArray       GetHLSPlaylist     (void);
    QByteArray       GetPlayerPage      (void);
    QByteArray       GetDashPlaylist    (void);

  private:
    Q_DISABLE_COPY(TorcCameraOutput)
    TorcCameraThread *m_thread;
    QReadWriteLock    m_threadLock;
    TorcCameraParams  m_params;
    QQueue<int>       m_segments;
    QReadWriteLock    m_segmentLock;
    QDateTime         m_cameraStartTime;
};

class TorcCameraOutputs final : public TorcDeviceHandler
{
  public:
    TorcCameraOutputs();

    static TorcCameraOutputs *gCameraOutputs;

    void Create  (const QVariantMap &Details) override;
    void Destroy (void) override;

  private:
    QHash<QString, TorcCameraOutput*> m_cameras;
};


#endif // TORCCAMERAOUTPUT_H
