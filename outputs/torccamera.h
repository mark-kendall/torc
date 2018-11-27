#ifndef TORCCAMERA_H
#define TORCCAMERA_H

// Qt
#include <QObject>

// Torc
#include "torcmaths.h"
#include "torcsegmentedringbuffer.h"
#include "ffmpeg/torcmuxer.h"

// FFmpeg
#include <libavcodec/avcodec.h>

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
#define VIDEO_DRIFT_SHORT    60 // short term drift average
#define VIDEO_DRIFT_LONG     (60*5) // long term drift average
#define VIDEO_H264_PROFILE   FF_PROFILE_H264_MAIN

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

Q_DECLARE_METATYPE(TorcCameraParams)

class TorcCameraDevice : public QObject
{
    Q_OBJECT

  public:
    explicit TorcCameraDevice(const TorcCameraParams &Params);
    virtual ~TorcCameraDevice();

    virtual bool     Setup           (void);
    virtual bool     Start           (void) = 0;
    virtual bool     Stop            (void) = 0;
    QByteArray       GetSegment      (int Segment);
    QByteArray       GetInitSegment  (void);

  public slots:
    virtual void     TakeStills      (uint Count);
    virtual void     StreamVideo     (bool Video) = 0;

  signals:
    void             WritingStarted  (void);
    void             WritingStopped  (void);
    void             SegmentRemoved  (int Segment);
    void             InitSegmentReady(void);
    void             SegmentReady    (int Segment);
    void             SetErrored      (bool Errored);
    void             StillReady      (const QString File);
    void             ParametersChanged (TorcCameraParams Params);

  protected:
    // streaming
    void             TrackDrift      (void);
    // stills
    virtual void     StartStill      (void) = 0;
    virtual bool     EnableStills    (uint Count);
    void             SaveStill       (void);
    void             SaveStillBuffer (quint32 Length, uint8_t* Data);
    void             ClearStillsBuffers (void);

    TorcCameraParams         m_params;

    // video/streaming
    TorcMuxer               *m_muxer;
    int                      m_videoStream;
    quint64                  m_frameCount;
    bool                     m_haveInitSegment;
    AVPacket                *m_bufferedPacket;
    TorcSegmentedRingBuffer *m_ringBuffer;
    QReadWriteLock           m_ringBufferLock;
    quint64                  m_referenceTime;
    int                      m_discardDrift;
    TorcAverage<double>      m_shortAverage;
    TorcAverage<double>      m_longAverage;

    // stills
    uint                     m_stillsRequired;
    uint                     m_stillsExpected;
    QList<QPair<quint32, uint8_t*> > m_stillsBuffers;

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
