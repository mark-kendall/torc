#ifndef TORCCAMERATHREAD_H
#define TORCCAMERATHREAD_H

// Torc
#include "torcreferencecounted.h"
#include "torcqthread.h"
#include "torccamera.h"

class TorcCameraOutput;

class TorcCameraThread final : public TorcQThread, public TorcReferenceCounter
{
    Q_OBJECT

  public:
    ~TorcCameraThread();

    static void       CreateOrDestroy(TorcCameraThread*& Thread, const QString &Type, const TorcCameraParams &Params = TorcCameraParams());
    void              Start          (void) override;
    void              Finish         (void) override;
    QByteArray        GetSegment     (int Segment);
    QByteArray        GetInitSegment (void);
    TorcCameraParams  GetParams      (void);
    void              SetParent      (TorcCameraOutput *Parent);

  public slots:
    void              StopWriting    (void);

  signals:
    void              WritingStarted (void);
    void              WritingStopped (void);
    void              InitSegmentReady(void);
    void              SegmentReady   (int);
    void              SegmentRemoved (int);
    void              CameraErrored  (bool);

  private:
    TorcCameraThread(const QString &Type, const TorcCameraParams &Params);
    Q_DISABLE_COPY(TorcCameraThread)
    bool              DownRef        (void) override;

    TorcCameraOutput *m_parent;
    QString           m_type;
    TorcCameraParams  m_params;
    TorcCameraDevice *m_camera;
    QReadWriteLock    m_cameraLock;
    bool              m_stop;
};


#endif // TORCCAMERATHREAD_H
