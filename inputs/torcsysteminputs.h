#ifndef TORCSYSTEMINPUTS_H
#define TORCSYSTEMINPUTS_H

// Torc
#include "torccentral.h"
#include "torcinput.h"

#define SYSTEM_INPUTS_STRING QStringLiteral("system")

class TorcSystemInputs final : public TorcDeviceHandler
{
  public:
    TorcSystemInputs();

    static TorcSystemInputs *gSystemInputs;

    void                     Create      (const QVariantMap &Details) override;
    void                     Destroy     (void) override;

  private:
    QMap<QString,TorcInput*> m_inputs;
};

#endif // TORCSYSTEMINPUTS_H
