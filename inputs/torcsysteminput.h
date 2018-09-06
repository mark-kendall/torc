#ifndef TORCSYSTEMINPUT_H
#define TORCSYSTEMINPUT_H

// Torc
#include "torcinput.h"

class TorcSystemInput Q_DECL_FINAL : public TorcInput
{
    Q_OBJECT

  public:
    TorcSystemInput(double Value, const QVariantMap &Details);
   ~TorcSystemInput();

    TorcInput::Type GetType(void);

  public slots:
    bool         event (QEvent *Event) Q_DECL_OVERRIDE;
};

#endif // TORCSYSTEMINPUT_H
