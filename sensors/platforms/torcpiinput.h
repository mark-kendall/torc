#ifndef TORCPIINPUT_H
#define TORCPIINPUT_H

// Qt
#include <QFile>

// Torc
#include "torcqthread.h"
#include "torcswitchsensor.h"

class TorcPiInput;

// this has to be declared here as we cannot have a Q_OBJECT class declared privately
class TorcPiInputThread : public TorcQThread
{
    Q_OBJECT

  public:
    TorcPiInputThread(TorcPiInput* Parent, int Pin);
    virtual ~TorcPiInputThread();

    void         Start    (void);
    void         Finish   (void);
    void         Stop     (void);

  signals:
    void         Changed  (double Value);

  protected:
    void         run      (void);
    void         Update   (void);
 
  private:
    TorcPiInput *m_parent;
    int          m_pin;
    bool         m_aborted;
    QFile        m_file;
};

class TorcPiInput : public TorcSwitchSensor
{
    Q_OBJECT

  public:
    TorcPiInput(int Pin, const QVariantMap &Details);
    virtual ~TorcPiInput();

    void               Start          (void);
    QStringList        GetDescription (void);

  private:
    int                m_pin;
    TorcPiInputThread *m_inputThread;
};

#endif // TORCPIINPUT_H
