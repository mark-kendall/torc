#ifndef TORCIOTLOGGER_H
#define TORCIOTLOGGER_H

// Qt
#include <QObject>
#include <QTimer>
#include <QDateTime>

// Torc
#include "torcnotifier.h"

class TorcNetworkRequest;

class TorcIOTLogger : public TorcNotifier
{
    Q_OBJECT

  public:
    explicit TorcIOTLogger(const QVariantMap &Details);
    virtual ~ TorcIOTLogger();

    QStringList GetDescription(void) override final;

  public slots:
    void         Notify        (const QVariantMap &Notification) override;
    void         DoNotify      (void);
    void         RequestReady  (TorcNetworkRequest* Request);

  signals:
    void         StartTimer    (int Milliseconds);
    void         TryNotify     (void);

  protected:
    virtual bool Initialise    (const QVariantMap &Details);
    virtual void ProcessRequest(TorcNetworkRequest* Request) = 0;
    virtual TorcNetworkRequest* CreateRequest(void) = 0;

  protected:
    QString      m_description;
    QTimer       m_timer;
    int          m_networkAbort;
    QString      m_apiKey;
    int          m_badRequestCount;
    int          m_maxBadRequests;
    int          m_maxUpdateInterval;
    QDateTime    m_lastUpdate;
    QList<TorcNetworkRequest*> m_requests;
    int                        m_maxFields;
    QMap<QString,int>          m_fields;
    QMap<int,QString>          m_reverseFields;
    QString                    m_fieldValues[32];
};

#endif // TORCIOTLOGGER_H
