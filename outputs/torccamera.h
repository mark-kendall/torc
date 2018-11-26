#ifndef TORCCAMERA_H
#define TORCCAMERA_H

// Qt
#include <QObject>

// Torc
#include "torcsegmentedringbuffer.h"

// NB these are also enforced in the XSD
#define VIDEO_WIDTH_MIN      640
#define VIDEO_WIDTH_MAX      1920
#define VIDEO_HEIGHT_MIN     480
#define VIDEO_HEIGHT_MAX     1080
#define VIDEO_FRAMERATE_MIN  1
#define VIDEO_FRAMERATE_MAX  60
#define VIDEO_BITRATE_MIN    128000
#define VIDEO_BITRATE_MAX    25000000
#define VIDEO_SEGMENT_TARGET 2  // 2 second segments
#define VIDEO_GOPDURA_TARGET 1  // with IDR every second
#define VIDEO_SEGMENT_NUMBER 10 // 10 segments for a total of 20 buffered seconds
#define VIDEO_SEGMENT_MAX    20
#define VIDEO_TIMEBASE       90000

class TorcCameraParams
{
  public:
    TorcCameraParams(void);
    explicit TorcCameraParams(const QVariantMap &Details);
    TorcCameraParams(const TorcCameraParams &Other);
    TorcCameraParams  Combine     (const TorcCameraParams &Add);
    TorcCameraParams& operator =  (const TorcCameraParams &Other);
    bool              operator == (const TorcCameraParams &Other) const;

    bool    IsVideo      (void) const;
    bool    IsStill      (void) const;
    bool    IsCompatible (const TorcCameraParams &Other) const;

    bool    m_valid;
    int     m_width;
    int     m_height;
    int     m_stride;
    int     m_sliceHeight;
    int     m_frameRate;
    int     m_bitrate;
    int     m_timebase;
    int     m_segmentLength;
    int     m_gopSize;
    QString m_videoCodec;
    QString m_contentDir;
};

class TorcCameraDevice : public QObject
{
    Q_OBJECT

  public:
    explicit TorcCameraDevice(const TorcCameraParams &Params);
    virtual ~TorcCameraDevice();

    virtual bool     Setup           (void) = 0;
    virtual bool     Start           (void) = 0;
    virtual bool     Stop            (void) = 0;
    QByteArray       GetSegment      (int Segment);
    QByteArray       GetInitSegment  (void);
    TorcCameraParams GetParams       (void);

  public slots:
    virtual void     TakeStills      (uint Count) = 0;
    virtual void     StreamVideo     (bool Video) = 0;

  signals:
    void             WritingStarted  (void);
    void             WritingStopped  (void);
    void             SegmentRemoved  (int Segment);
    void             InitSegmentReady(void);
    void             SegmentReady    (int Segment);
    void             SetErrored      (bool Errored);
    void             StillReady      (const QString File);

  protected:
    TorcCameraParams m_params;
    TorcSegmentedRingBuffer *m_ringBuffer;
    QReadWriteLock   m_ringBufferLock;

  private:
    Q_DISABLE_COPY(TorcCameraDevice)
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
