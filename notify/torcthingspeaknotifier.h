#ifndef TORCTHINGSPEAKNOTIFIER_H
#define TORCTHINGSPEAKNOTIFIER_H

// Torc
#include "torciotlogger.h"

class TorcThingSpeakNotifier final : public TorcIOTLogger
{
    Q_OBJECT

  public:
    explicit TorcThingSpeakNotifier(const QVariantMap &Details);
    virtual ~TorcThingSpeakNotifier() = default;

    void ProcessRequest(TorcNetworkRequest* Request) override;
    TorcNetworkRequest* CreateRequest(void) override;
};

#endif // TORCTHINGSPEAKNOTIFIER_H
