#ifndef TORCPISWITCHINPUT_H
#define TORCPISWITCHINPUT_H

// Qt
#include <QFile>

// Torc
#include "torcqthread.h"
#include "torcswitchinput.h"

class TorcPiSwitchInput;

// this has to be declared here as we cannot have a Q_OBJECT class declared privately
class TorcPiSwitchInputThread : public TorcQThread
{
    Q_OBJECT

  public:
    TorcPiSwitchInputThread(TorcPiSwitchInput* Parent, int Pin);
    virtual ~TorcPiSwitchInputThread();

    void         Start    (void);
    void         Finish   (void);
    void         Stop     (void);

  signals:
    void         Changed  (double Value);

  protected:
    void         run      (void);
    void         Update   (void);
 
  private:
    TorcPiSwitchInput *m_parent;
    int                m_pin;
    bool               m_aborted;
    QFile              m_file;
};

class TorcPiSwitchInput : public TorcSwitchInput
{
    Q_OBJECT

  public:
    TorcPiSwitchInput(int Pin, const QVariantMap &Details);
    virtual ~TorcPiSwitchInput();

    void               Start          (void);
    QStringList        GetDescription (void);

  private:
    int                      m_pin;
    TorcPiSwitchInputThread *m_inputThread;
};

#endif // TORCPISWITCHINPUT_H
