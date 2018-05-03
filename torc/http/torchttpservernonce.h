#ifndef TORCHTTPSERVERNONCE_H
#define TORCHTTPSERVERNONCE_H

//Qt
#include <QMutex>
#include <QDateTime>

#define TORC_REALM QString("Torc")
#define DEFAULT_NONCE_LIFETIME_SECONDS  60
#define DEFAULT_NONCE_LIFETIME_REQUESTS 0  // unlimited

class TorcHTTPServerNonce
{
  public:
    static quint64                             gServerNoncesCount;
    static QHash<QString,TorcHTTPServerNonce*> gServerNonces;
    static QMutex                             *gServerNoncesLock;

    static bool AddNonce(TorcHTTPServerNonce* Nonce);
    static void RemoveNonce(TorcHTTPServerNonce* Nonce);
    static void ExpireNonces(void);
    static bool CheckAuthentication(const QString &Username, const QString &Password,
                                    const QString &Header,   const QString &Method,
                                    const QString &URI,      bool &Stale);
  public:
    TorcHTTPServerNonce();
    TorcHTTPServerNonce(quint64 LifetimeInSeconds, quint64 LifetimeInRequests = DEFAULT_NONCE_LIFETIME_REQUESTS);
   ~TorcHTTPServerNonce();

    QString GetNonce(void) const;
    QString GetOpaque(void) const;
    bool UseOnce(quint64 ThisUse, bool &Stale);
    bool IsOutOfDate(const QDateTime &Current);

  private:
    void CreateNonce(void);

  private:
    bool       m_expired;
    QString    m_nonce;
    QString    m_opaque;
    quint64    m_startMs;
    QDateTime  m_startTime;
    quint64    m_useCount;
    quint64    m_lifetimeInSeconds;
    quint64    m_lifetimeInRequests;
};

#endif // TORCHTTPSERVERNONCE_H
