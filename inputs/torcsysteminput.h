#ifndef TORCSYSTEMINPUT_H
#define TORCSYSTEMINPUT_H

// Torc
#include "torcinput.h"

class TorcSystemInput final : public TorcInput
{
    Q_OBJECT

  public:
    TorcSystemInput(double Value, const QVariantMap &Details);
   ~TorcSystemInput();

    TorcInput::Type GetType        (void) override;
    QStringList     GetDescription (void) override;

  public slots:
    bool            event          (QEvent *Event) override;

  private:
    uint            m_shutdownDelay;
};

#endif // TORCSYSTEMINPUT_H
