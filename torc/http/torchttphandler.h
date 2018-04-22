#ifndef TORCHTTPHANDLER_H
#define TORCHTTPHANDLER_H

// Qt
#include <QString>
#include <QVariantMap>

// Torc
#include "torchttprequest.h"

class TorcHTTPServer;
class TorcHTTPConnection;

#define STATIC_DIRECTORY QString("/css,/img,/fonts,/js")
#define DYNAMIC_DIRECTORY QString("/content/")

class TorcHTTPHandler
{
  public:
    TorcHTTPHandler(const QString &Signature, const QString &Name);
    virtual ~TorcHTTPHandler();

    QString             Signature          (void) const;
    bool                GetRecursive       (void) const;
    QString             Name               (void) const;
    virtual void        ProcessHTTPRequest (TorcHTTPRequest *Request, TorcHTTPConnection *Connection) = 0;
    virtual QVariantMap ProcessRequest     (const QString &Method, const QVariant &Parameters, QObject *Connection);

  protected:
    void                HandleOptions      (TorcHTTPRequest *Request, int Allowed);
    void                HandleFile         (TorcHTTPRequest *Request, const QString &Filename, int Cache);

  protected:
    QString             m_signature;
    bool                m_recursive;
    QString             m_name;
};

#endif // TORCHTTPHANDLER_H
