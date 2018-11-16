#ifndef TORCCAMERATHREAD_H
#define TORCCAMERATHREAD_H

// Torc
#include "torcqthread.h"
#include "torccamera.h"

class TorcCameraOutput;

class TorcCameraThread final : public TorcQThread
{
    Q_OBJECT

  public:
    TorcCameraThread(TorcCameraOutput *Parent, const QString &Type, const TorcCameraParams &Params);
    ~TorcCameraThread();

    void              Start          (void) override;
    void              Finish         (void) override;
    QByteArray        GetSegment     (int Segment);
    QByteArray        GetInitSegment (void);
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


#endif // TORCCAMERATHREAD_H
