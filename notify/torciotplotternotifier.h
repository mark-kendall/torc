#ifndef TORCIOTPLOTTERNOTIFIER_H
#define TORCIOTPLOTTERNOTIFIER_H

//Torc
#include "torciotlogger.h"

class TorcIoTPlotterNotifier : public TorcIOTLogger
{
    Q_OBJECT

  public:
    explicit TorcIoTPlotterNotifier(const QVariantMap &Details);
    virtual ~TorcIoTPlotterNotifier();

    virtual void ProcessRequest(TorcNetworkRequest* Request);
    virtual TorcNetworkRequest* CreateRequest(void);

  private:
    QString m_feedId;
};

#endif // TORCIOTPLOTTERNOTIFIER_H
