#ifndef TORCTHINGSPEAKNOTIFIER_H
#define TORCTHINGSPEAKNOTIFIER_H

// Torc
#include "torciotlogger.h"

class TorcThingSpeakNotifier : public TorcIOTLogger
{
    Q_OBJECT

  public:
    TorcThingSpeakNotifier(const QVariantMap &Details);
    virtual ~TorcThingSpeakNotifier();

    virtual void ProcessRequest(TorcNetworkRequest* Request);
    virtual TorcNetworkRequest* CreateRequest(void);
};

#endif // TORCTHINGSPEAKNOTIFIER_H
