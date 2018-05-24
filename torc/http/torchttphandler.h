#ifndef TORCHTTPHANDLER_H
#define TORCHTTPHANDLER_H

// Qt
#include <QString>
#include <QVariantMap>

// Torc
#include "torchttprequest.h"

class TorcHTTPServer;

#define STATIC_DIRECTORY QString("/css,/img,/fonts,/js")
#define DYNAMIC_DIRECTORY QString("/content/")
#define UPNP_DIRECTORY   QString("/upnp/")

class TorcHTTPHandler
{
  public:
    TorcHTTPHandler(const QString &Signature, const QString &Name);
    virtual ~TorcHTTPHandler();

    QString             Signature          (void) const;
    bool                GetRecursive       (void) const;
    QString             Name               (void) const;
    virtual void        ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) = 0;
    virtual QVariantMap ProcessRequest     (const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated);

  protected:
    static bool         MethodIsAuthorised (TorcHTTPRequest &Request, int Allowed);
    static void         HandleOptions      (TorcHTTPRequest &Request, int Allowed);
    static void         HandleFile         (TorcHTTPRequest &Request, const QString &Filename, int Cache);

  protected:
    QString             m_signature;
    bool                m_recursive;
    QString             m_name;
};

#endif // TORCHTTPHANDLER_H
