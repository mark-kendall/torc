#ifndef TORCCAMERA_H
#define TORCCAMERA_H

// Qt
#include <QObject>

// Torc
#include "torcqthread.h"
#include "torcsegmentedringbuffer.h"
#include "torcoutput.h"

// NB these are also enforced in the XSD
#define VIDEO_WIDTH_MIN      640
#define VIDEO_WIDTH_MAX      1920
#define VIDEO_HEIGHT_MIN     480
#define VIDEO_HEIGHT_MAX     1080
#define VIDEO_FRAMERATE_MIN  1
#define VIDEO_FRAMERATE_MAX  60
#define VIDEO_BITRATE_MIN    128000
#define VIDEO_BITRATE_MAX    25000000
#define VIDEO_SEGMENT_TARGET 2
#define VIDEO_GOPDURA_TARGET 1 //2 second segment with IDR every second
#define VIDEO_SEGMENT_NUMBER 10

#define DASH_PLAYLIST        QString("dash.mpd")
#define HLS_PLAYLIST_MAST    QString("master.m3u8")
#define HLS_PLAYLIST         QString("playlist.m3u8")
#define VIDEO_PAGE           QString("video.html")

#define VIDEO_CODEC_ISO      QString("avc1.4d0028") // AVC Main Level 4
#define AUDIO_CODEC_ISO      QString("mp4a.40.2")   // AAC LC

class TorcCameraParams
{
  public:
    TorcCameraParams(void);
    TorcCameraParams(const QVariantMap &Details);
    TorcCameraParams(const TorcCameraParams &Other);

    bool    m_valid;
    int     m_width;
    int     m_height;
    int     m_frameRateNum;
    int     m_frameRateDiv;
    int     m_bitrate;
    int     m_timebase;
    int     m_segmentLength;
    int     m_gopSize;
    QString m_model;
    QString m_videoCodec;
};

class TorcCameraDevice : public QObject
{
    Q_OBJECT

  public:
    TorcCameraDevice(const TorcCameraParams &Params);
    virtual ~TorcCameraDevice();

    virtual bool     Setup           (void) = 0;
    virtual bool     WriteFrame      (void) = 0;
    virtual bool     Stop            (void) = 0;
    QByteArray*      GetSegment      (int Segment);
    QByteArray*      GetInitSegment  (void);
    TorcCameraParams GetParams       (void);

  signals:
    void             SegmentRemoved  (int Segment);
    void             SegmentReady    (int Segment);
    void             SetErrored      (bool Errored);

  protected:
    TorcCameraParams m_params;
    TorcSegmentedRingBuffer *m_ringBuffer;

  private:
    Q_DISABLE_COPY(TorcCameraDevice)
};

class TorcCameraOutput;

class TorcCameraThread Q_DECL_FINAL : public TorcQThread
{
    Q_OBJECT

  public:
    TorcCameraThread(TorcCameraOutput *Parent, const QString &Type, const TorcCameraParams &Params);
    ~TorcCameraThread();

    void              Start          (void) Q_DECL_OVERRIDE;
    void              Finish         (void) Q_DECL_OVERRIDE;
    QByteArray*       GetSegment     (int Segment);
    QByteArray*       GetInitSegment (void);
    TorcCameraParams  GetParams      (void);

  public slots:
    void              StopWriting    (void);

  signals:
    void              WritingStarted (void);
    void              WritingStopped (void);

  private:
    Q_DISABLE_COPY(TorcCameraThread)
    TorcCameraOutput *m_parent;
    QString           m_type;
    TorcCameraParams  m_params;
    TorcCameraDevice *m_camera;
    QReadWriteLock    m_cameraLock;
    bool              m_stop;
};

class TorcCameraOutput Q_DECL_FINAL : public TorcOutput
{
    Q_OBJECT

  public:
    TorcCameraOutput(const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcCameraOutput();

    TorcOutput::Type GetType            (void) Q_DECL_OVERRIDE;
    void             Start              (void) Q_DECL_OVERRIDE;
    void             Stop               (void) Q_DECL_OVERRIDE;
    QString          GetPresentationURL (void) Q_DECL_OVERRIDE;

  public slots:
    void             ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress,
                                         int LocalPort, TorcHTTPRequest &Request) Q_DECL_OVERRIDE;
    void             WritingStarted     (void);
    void             WritingStopped     (void);
    void             CameraErrored      (bool Errored);
    void             SegmentRemoved     (int Segment);
    void             SegmentReady       (int Segment);

  private:
    QByteArray*      GetMasterPlaylist  (void);
    QByteArray*      GetHLSPlaylist     (void);
    QByteArray*      GetPlayerPage      (void);
    QByteArray*      GetDashPlaylist    (void);

  private:
    Q_DISABLE_COPY(TorcCameraOutput)
    TorcCameraThread *m_thread;
    QReadWriteLock    m_threadLock;
    TorcCameraParams  m_params;
    QQueue<int>       m_segments;
    QReadWriteLock    m_segmentLock;
    QDateTime         m_cameraStartTime;
};

class TorcCameraOutputs Q_DECL_FINAL : public TorcDeviceHandler
{
  public:
    TorcCameraOutputs();
    ~TorcCameraOutputs();

    static TorcCameraOutputs *gCameraOutputs;

    void Create  (const QVariantMap &Details) Q_DECL_OVERRIDE;
    void Destroy (void) Q_DECL_OVERRIDE;

  private:
    QHash<QString, TorcCameraOutput*> m_cameras;
};

class TorcCameraFactory
{
  public:
    TorcCameraFactory();
    virtual ~TorcCameraFactory();

    static TorcCameraDevice*    GetCamera            (const QString &Type, const TorcCameraParams &Params);
    static TorcCameraFactory*   GetTorcCameraFactory (void);
    TorcCameraFactory*          NextFactory          (void) const;
    virtual bool                CanHandle            (const QString &Type, const TorcCameraParams &Params) = 0;
    virtual TorcCameraDevice*   Create               (const QString &Type, const TorcCameraParams &Params) = 0;

  protected:
    static TorcCameraFactory*   gTorcCameraFactory;
    TorcCameraFactory*          nextTorcCameraFactory;

  private:
    Q_DISABLE_COPY(TorcCameraFactory)
};

#endif // TORCCAMERA_H
