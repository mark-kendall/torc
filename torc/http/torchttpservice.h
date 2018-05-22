#ifndef TORCSERVICE_H
#define TORCSERVICE_H

// Qt
#include <QMap>
#include <QMutex>
#include <QMetaObject>
#include <QCoreApplication>

// Torc
#include "torchttphandler.h"
#include "torchttprequest.h"

class TorcHTTPServer;
class MethodParameters;

class TorcHTTPService : public TorcHTTPHandler
{
    Q_DECLARE_TR_FUNCTIONS(TorcHTTPService)

  public:
    TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                    const QMetaObject &MetaObject, const QString &Blacklist = QString(""));
    virtual ~TorcHTTPService();

    void         ProcessHTTPRequest       (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) Q_DECL_OVERRIDE;
    QVariantMap  ProcessRequest           (const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated) Q_DECL_OVERRIDE;
    QString      GetMethod                (int Index);
    QVariant     GetProperty              (int Index);
    QVariantMap  GetServiceDetails        (void);

    virtual QString GetUIName             (void);

  protected:
    static bool  MethodIsAuthorised       (TorcHTTPRequest &Request, int Allowed);
    void         EnableMethod             (const QString &Method);
    void         DisableMethod            (const QString &Method);
    void         HandleSubscriberDeleted  (QObject* Subscriber);

  private:
    QObject                               *m_parent;
    QString                                m_version;
    QMetaObject                            m_metaObject;
    QMap<QString,MethodParameters*>        m_methods;
    QMap<int,int>                          m_properties;
    QList<QObject*>                        m_subscribers;
    QMutex                                 m_subscriberLock;

  private:
    Q_DISABLE_COPY(TorcHTTPService)
};

Q_DECLARE_METATYPE(TorcHTTPService*);
#endif // TORCSERVICE_H
