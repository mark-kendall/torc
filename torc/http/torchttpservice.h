#ifndef TORCSERVICE_H
#define TORCSERVICE_H

// Qt
#include <QMap>
#include <QMutex>
#include <QReadWriteLock>
#include <QMetaObject>
#include <QCoreApplication>

// Torc
#include "torchttphandler.h"
#include "torchttprequest.h"

#define TORC_SERVICE_VERSION QStringLiteral("version")

class TorcHTTPServer;
class MethodParameters;

class TorcHTTPService : public TorcHTTPHandler
{
    Q_DECLARE_TR_FUNCTIONS(TorcHTTPService)

  public:
    TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                    const QMetaObject &MetaObject, const QString &Blacklist = QStringLiteral(""));
    virtual ~TorcHTTPService();

    void         ProcessHTTPRequest       (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) override;
    QVariantMap  ProcessRequest           (const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated) override;
    QString      GetMethod                (int Index);
    QVariant     GetProperty              (int Index);
    QVariantMap  GetServiceDetails        (void);

    virtual QString GetUIName             (void);
    virtual QString GetPresentationURL    (void);

  protected:
    void         HandleSubscriberDeleted  (QObject* Subscriber);
    // this is a generic lock around data that may be accessed fro multiple sockets/threads
    // use with care to ensure maximum concurrency.
    QReadWriteLock                         m_httpServiceLock;

  private:
    QObject                               *m_parent;
    QString                                m_version;
    QMap<QString,MethodParameters*>        m_methods;
    QMap<int,int>                          m_properties;
    QList<QObject*>                        m_subscribers;
    QMutex                                 m_subscriberLock;

  private:
    Q_DISABLE_COPY(TorcHTTPService)
};

Q_DECLARE_METATYPE(TorcHTTPService*)
#endif // TORCSERVICE_H
