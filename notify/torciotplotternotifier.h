#ifndef TORCIOTPLOTTERNOTIFIER_H
#define TORCIOTPLOTTERNOTIFIER_H

//Torc
#include "torciotlogger.h"

class TorcIoTPlotterNotifier Q_DECL_FINAL : public TorcIOTLogger
{
    Q_OBJECT

  public:
    explicit TorcIoTPlotterNotifier(const QVariantMap &Details);
    virtual ~TorcIoTPlotterNotifier();

    void ProcessRequest(TorcNetworkRequest* Request) Q_DECL_OVERRIDE;
    TorcNetworkRequest* CreateRequest(void) Q_DECL_OVERRIDE;

  private:
    QString m_feedId;
};

#endif // TORCIOTPLOTTERNOTIFIER_H
