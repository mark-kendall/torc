#ifndef TORCQTHREAD_H
#define TORCQTHREAD_H

// Qt
#include <QThread>

class TorcQThread : public QThread
{
    Q_OBJECT

  public:
    static void     SetMainThread    (void);
    static bool     IsMainThread     (void);
    static void     InitRand         (void);

  public:
    explicit TorcQThread(const QString &Name);
    virtual ~TorcQThread() = default;
    
  signals:
    void            Started          (void);
    void            Finished         (void);

  public:
    virtual void    Start            (void) = 0;
    virtual void    Finish           (void) = 0;

  protected:
    virtual void    run              (void);
    void            Initialise       (void);
    void            Deinitialise     (void);
};

#endif // TORCQTHREAD_H
