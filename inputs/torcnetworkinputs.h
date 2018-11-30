#ifndef TORCNETWORKINPUTS_H
#define TORCNETWORKINPUTS_H

// Qt
#include <QMutex>

// Torc
#include "torccentral.h"
#include "torcoutput.h"
#include "torcinput.h"

#define NETWORK_INPUTS_STRING QString("network")
#define CONSTANT_INPUTS_STRING QString("constant")

class TorcNetworkInputs final : public TorcDeviceHandler
{
  public:
    TorcNetworkInputs();
    ~TorcNetworkInputs() = default;

    static TorcNetworkInputs*   gNetworkInputs;

    void                        Create      (const QVariantMap &Details) override;
    void                        Destroy     (void) override;

  private:
    QMap<QString,TorcInput*>    m_inputs;
    QMap<QString,TorcOutput*>   m_outputs;
};

#endif // TORCNETWORKINPUTS_H
