#ifndef TORCPISWITCHINPUT_H
#define TORCPISWITCHINPUT_H

// Qt
#include <QFile>

// Torc
#include "torcqthread.h"
#include "torcswitchinput.h"

class TorcPiSwitchInput;

class TorcPiSwitchInputThread final : public TorcQThread
{
    Q_OBJECT
  public:
    TorcPiSwitchInputThread(TorcPiSwitchInput* Parent, int Pin);
    virtual ~TorcPiSwitchInputThread() = default;

    void         Start    (void) override;
    void         Finish   (void) override;
    void         Stop     (void);

  signals:
    void         Changed  (double Value);

  protected:
    void         run      (void) override;
    void         Update   (void);
 
  private:
    Q_DISABLE_COPY(TorcPiSwitchInputThread)
    TorcPiSwitchInput *m_parent;
    int                m_pin;
    bool               m_aborted;
    QFile              m_file;
};

class TorcPiSwitchInput final : public TorcSwitchInput
{
    Q_OBJECT

  public:
    TorcPiSwitchInput(int Pin, const QVariantMap &Details);
    virtual ~TorcPiSwitchInput();

    void               Start          (void) override;
    QStringList        GetDescription (void) override;

  private:
    Q_DISABLE_COPY(TorcPiSwitchInput)
    int                      m_pin;
    TorcPiSwitchInputThread *m_inputThread;
};

#endif // TORCPISWITCHINPUT_H
