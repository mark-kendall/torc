#ifndef TORCHTTPSERVICES_H
#define TORCHTTPSERVICES_H

// Qt
#include <QObject>
#include <QVariant>

// Torc
#include "torchttpservice.h"

class TorcHTTPServer;
class TorcHTTPRequest;

class TorcHTTPServices : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",           "1.0.0")
    Q_CLASSINFO("GetServiceList",    "type=services")
    Q_CLASSINFO("GetStartTime",      "type=starttime")
    Q_CLASSINFO("GetPriority",       "type=priority")
    Q_CLASSINFO("GetUuid",           "type=uuid")
    Q_CLASSINFO("GetDetails",        "type=details")
    Q_CLASSINFO("GetWebSocketToken", "type=accesstoken,methods=GET+AUTH")
    Q_CLASSINFO("IsSecure",          "type=secure,methods=GET")

    Q_PROPERTY(QMap         serviceList        READ GetServiceList        NOTIFY ServiceListChanged)
    Q_PROPERTY(QVariantList returnFormats      READ GetReturnFormats      CONSTANT)
    Q_PROPERTY(QVariantList webSocketProtocols READ GetWebSocketProtocols CONSTANT)

  public:
    explicit TorcHTTPServices(TorcHTTPServer *Server);
    virtual ~TorcHTTPServices() = default;

    static QString GetVersion           (void);
    void           ProcessHTTPRequest   (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) override;
    QVariantMap    ProcessRequest       (const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated) override;
    QString        GetUIName            (void) override;

  signals:
    void           ServiceListChanged   (void);

  public slots:
    void           SubscriberDeleted    (QObject *Subscriber);
    QVariantMap    GetDetails           (void);
    QVariantMap    GetServiceList       (void);
    QVariantMap    GetServiceDescription(const QString &Service);
    QVariantList   GetReturnFormats     (void);
    QVariantList   GetWebSocketProtocols (void);
    qint64         GetStartTime         (void);
    int            GetPriority          (void);
    QString        GetUuid              (void);
    QString        GetWebSocketToken    (void);
    bool           IsSecure             (void);
    void           HandlersChanged      (void);

  private:
    QVariantMap    serviceList;   // dummy
    QVariantList   returnFormats;
    QVariantList   webSocketProtocols; // dummy
};

#endif // TORCHTTPSERVICES_H
