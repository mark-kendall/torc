#ifndef TORCIOTPLOTTERNOTIFIER_H
#define TORCIOTPLOTTERNOTIFIER_H

//Torc
#include "torciotlogger.h"

class TorcIoTPlotterNotifier final : public TorcIOTLogger
{
    Q_OBJECT

  public:
    explicit TorcIoTPlotterNotifier(const QVariantMap &Details);
    virtual ~TorcIoTPlotterNotifier() = default;

    void ProcessRequest(TorcNetworkRequest* Request) override;
    TorcNetworkRequest* CreateRequest(void) override;

  private:
    QString m_feedId;
};

#endif // TORCIOTPLOTTERNOTIFIER_H
