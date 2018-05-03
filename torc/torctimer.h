#ifndef TORCTIMER_H_
#define TORCTIMER_H_

// Qt
#include <QTime>

class TorcTimer
{
  public:
    TorcTimer();

    void Start(void);
    int  Restart(void);
    int  Elapsed(void);
    void Stop(void);
    bool IsRunning(void) const;

  private:
    QTime m_timer;
    bool  m_running;
};

#endif

