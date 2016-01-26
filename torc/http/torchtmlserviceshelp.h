#ifndef TORCHTMLSERVICESHELP_H
#define TORCHTMLSERVICESHELP_H

// Qt
#include <QObject>
#include <QVariant>

// Torc
#include "torchttpservice.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;

class TorcHTMLServicesHelp : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",           "1.0.0")
    Q_CLASSINFO("GetServiceList",    "type=services")
    Q_CLASSINFO("GetStartTime",      "type=starttime")
    Q_CLASSINFO("GetPriority",       "type=priority")
    Q_CLASSINFO("GetUuid",           "type=uuid")
    Q_CLASSINFO("GetDetails",        "type=details")
    Q_CLASSINFO("GetWebSocketToken", "type=accesstoken")

    Q_PROPERTY(QMap serviceList READ GetServiceList   NOTIFY ServiceListChanged)
    Q_PROPERTY(QVariantList returnFormats READ GetReturnFormats CONSTANT)
    Q_PROPERTY(QVariantList webSocketProtocols READ GetWebSocketProtocols CONSTANT)

  public:
    TorcHTMLServicesHelp(TorcHTTPServer *Server);
    virtual ~TorcHTMLServicesHelp();

    void           ProcessHTTPRequest   (TorcHTTPRequest *Request, TorcHTTPConnection* Connection);
    QString        GetUIName            (void);

  signals:
    void           ServiceListChanged   (void);

  public slots:
    void           SubscriberDeleted    (QObject *Subscriber);
    QVariantMap    GetDetails           (void);
    QVariantMap    GetServiceList       (void);
    QVariantList   GetReturnFormats     (void);
    QVariantList   GetWebSocketProtocols (void);
    qint64         GetStartTime         (void);
    int            GetPriority          (void);
    QString        GetUuid              (void);
    QString        GetWebSocketToken    (void);
    void           HandlersChanged      (void);

  private:
    QVariantMap    serviceList;   // dummy
    QVariantList   returnFormats;
    QVariantList   webSocketProtocols; // dummy
};

#endif // TORCHTMLSERVICESHELP_H
