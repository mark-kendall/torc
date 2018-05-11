#ifndef TORCHTTPSERVERNONCE_H
#define TORCHTTPSERVERNONCE_H

//Qt
#include <QMutex>
#include <QDateTime>

// Torc
#include "torchttprequest.h"

#define TORC_REALM QString("Torc")
#define DEFAULT_NONCE_LIFETIME_SECONDS  10 // expire 10 seconds after issue or last use
#define DEFAULT_NONCE_LIFETIME_REQUESTS 0  // unlimited uses

class TorcHTTPServerNonce
{
  public:
    static void ProcessDigestAuth (TorcHTTPRequest &Request, const QString &Username = QString(),  const QString &Password = QString());

    TorcHTTPServerNonce();
    TorcHTTPServerNonce(const QDateTime &Time);
   ~TorcHTTPServerNonce();

    QString     GetOpaque(void) const;
    bool        UseOnce(quint64 ClientCount, const QDateTime &Current);
    bool        IsOutOfDate(const QDateTime &Current);

  private:
    bool       m_expired;
    QString    m_opaque;
    quint64    m_startMs;
    QDateTime  m_startTime;
    quint64    m_useCount;
    quint64    m_lifetimeInSeconds;
    quint64    m_lifetimeInRequests;
};

#endif // TORCHTTPSERVERNONCE_H
