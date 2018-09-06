#ifndef TORCSYSTEMINPUTS_H
#define TORCSYSTEMINPUTS_H

// Torc
#include "torccentral.h"
#include "torcinput.h"

#define SYSTEM_INPUTS_STRING QString("system")

class TorcSystemInputs Q_DECL_FINAL : public TorcDeviceHandler
{
  public:
    TorcSystemInputs();
   ~TorcSystemInputs();

    static TorcSystemInputs *gSystemInputs;

    void                     Create      (const QVariantMap &Details) Q_DECL_OVERRIDE;
    void                     Destroy     (void) Q_DECL_OVERRIDE;

  private:
    QMap<QString,TorcInput*> m_inputs;
};

#endif // TORCSYSTEMINPUTS_H