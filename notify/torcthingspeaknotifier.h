#ifndef TORCTHINGSPEAKNOTIFIER_H
#define TORCTHINGSPEAKNOTIFIER_H

// Torc
#include "torciotlogger.h"

class TorcThingSpeakNotifier Q_DECL_FINAL : public TorcIOTLogger
{
    Q_OBJECT

  public:
    explicit TorcThingSpeakNotifier(const QVariantMap &Details);
    virtual ~TorcThingSpeakNotifier();

    void ProcessRequest(TorcNetworkRequest* Request) Q_DECL_OVERRIDE;
    TorcNetworkRequest* CreateRequest(void) Q_DECL_OVERRIDE;
};

#endif // TORCTHINGSPEAKNOTIFIER_H
