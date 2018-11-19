#ifndef TORCCAMERATHREAD_H
#define TORCCAMERATHREAD_H

// Torc
#include "torcreferencecounted.h"
#include "torcqthread.h"
#include "torccamera.h"

class TorcCameraVideoOutput;
class TorcCameraStillsOutput;

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
    void              SetVideoParent (TorcCameraVideoOutput *Parent);
    void              SetStillsParent(TorcCameraStillsOutput *Parent);

  signals:
    void              StreamVideo    (bool Video);
    void              WritingStarted (void);
    void              WritingStopped (void);
    void              InitSegmentReady(void);
    void              SegmentReady   (int);
    void              SegmentRemoved (int);
    void              CameraErrored  (bool);
    void              StillReady     (const QString File);
    void              TakeStills     (uint Count);

  private:
    TorcCameraThread(const QString &Type, const TorcCameraParams &Params);
    Q_DISABLE_COPY(TorcCameraThread)
    bool              DownRef        (void) override;

    QString           m_type;
    TorcCameraParams  m_params;
    TorcCameraDevice *m_camera;
    QReadWriteLock    m_cameraLock;
};


#endif // TORCCAMERATHREAD_H
