#ifndef TORCNETWORKINPUTS_H
#define TORCNETWORKINPUTS_H

// Qt
#include <QMutex>

// Torc
#include "torccentral.h"
#include "torcoutput.h"
#include "torcinput.h"

#define NETWORK_INPUTS_STRING QString("network")

class TorcNetworkInputs : public TorcDeviceHandler
{
  public:
    TorcNetworkInputs();
   ~TorcNetworkInputs();

    static TorcNetworkInputs*   gNetworkInputs;

    void                        Create      (const QVariantMap &Details);
    void                        Destroy     (void);

  private:
    QMap<QString,TorcInput*>    m_inputs;
    QMap<QString,TorcOutput*>   m_outputs;
};

#endif // TORCNETWORKINPUTS_H
