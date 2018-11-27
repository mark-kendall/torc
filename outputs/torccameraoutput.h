#ifndef TORCCAMERAOUTPUT_H
#define TORCCAMERAOUTPUT_H

// Torc
#include "torcoutput.h"
#include "torccamera.h"

#define DASH_PLAYLIST        QString("dash.mpd")
#define HLS_PLAYLIST_MAST    QString("master.m3u8")
#define HLS_PLAYLIST         QString("playlist.m3u8")
#define VIDEO_PAGE           QString("video.html")

class TorcCameraThread;
class TorcNetworkRequest;
class TorcCameraOutputs;

class TorcCameraOutput : public TorcOutput
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")

    friend class TorcCameraOutputs;

  public:
    TorcCameraOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details,
                     QObject *Output, const QMetaObject &MetaObject, const QString &Blacklist = QString(""));
    virtual ~TorcCameraOutput();

  public slots:
    virtual void CameraErrored (bool Errored) = 0;
    void         ParamsChanged (TorcCameraParams Params);

  protected:
    TorcCameraParams& GetParams (void);
    void              SetParams (TorcCameraParams &Params);

  public:
    TorcCameraThread *m_thread;
    QReadWriteLock    m_threadLock;
    TorcCameraParams  m_params;

  private:
   Q_DISABLE_COPY(TorcCameraOutput)
};

class TorcCameraStillsOutput final : public TorcCameraOutput
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_PROPERTY(QStringList stillsList READ GetStillsList NOTIFY StillsListChanged(QStringList))

  public:
    TorcCameraStillsOutput(const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcCameraStillsOutput();

    TorcOutput::Type GetType            (void) override;
    void             Start              (void) override;
    void             Stop               (void) override;

  public slots:
    void             CameraErrored     (bool Errored) override;
    void             StillReady        (const QString File);
    QStringList      GetStillsList     (void);

  signals:
    void             TakeStills        (uint Count);
    void             StillsListChanged (QStringList);

 private:
    Q_DISABLE_COPY(TorcCameraStillsOutput)
    QStringList       stillsList; // adding MEMBER to property deems it writeable ??
    QStringList       m_stillsList;
    QString           m_stillsDirectory;

};

class TorcCameraVideoOutput final : public TorcCameraOutput
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")

  public:
    TorcCameraVideoOutput(const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcCameraVideoOutput();

    TorcOutput::Type GetType            (void) override;
    void             Start              (void) override;
    void             Stop               (void) override;
    QString          GetPresentationURL (void) override;
    void             ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress,
                                         int LocalPort, TorcHTTPRequest &Request) override;
  public slots:
    void             WritingStarted     (void);
    void             WritingStopped     (void);
    void             CameraErrored      (bool Errored) override;
    void             SegmentRemoved     (int Segment);
    void             InitSegmentReady   (void);
    void             SegmentReady       (int Segment);
    void             TimeCheck          (void);
    void             RequestReady       (TorcNetworkRequest *Request);

  signals:
    void             StreamVideo        (bool Video);
    void             CheckTime          (void);

  private:
    QByteArray       GetMasterPlaylist  (void);
    QByteArray       GetHLSPlaylist     (void);
    QByteArray       GetPlayerPage      (void);
    QByteArray       GetDashPlaylist    (void);

  private:
    Q_DISABLE_COPY(TorcCameraVideoOutput)
    QQueue<int>         m_segments;
    QReadWriteLock      m_segmentLock;
    QDateTime           m_cameraStartTime;
    int                 m_networkTimeAbort;
    TorcNetworkRequest *m_networkTimeRequest;
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
