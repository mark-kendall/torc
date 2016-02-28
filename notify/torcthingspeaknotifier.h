#ifndef TORCTHINGSPEAKNOTIFIER_H
#define TORCTHINGSPEAKNOTIFIER_H

// Qt
#include <QTimer>
#include <QDateTime>

// Torc
#include "torcnotifier.h"

class TorcNetworkRequest;

class TorcThingSpeakNotifier : public TorcNotifier
{
    Q_OBJECT

  public:
    TorcThingSpeakNotifier(const QVariantMap &Details);
    ~TorcThingSpeakNotifier();

  public slots:
    void RequestReady         (TorcNetworkRequest* Request);
    void Notify               (const QVariantMap &Notification);
    void DoNotify             (void);

  signals:
    void StartTimer           (int Milliseconds);

  private:
    QTimer                    *m_timer;
    int                        m_networkAbort;
    QString                    m_apiKey;
    QMap<QString,int>          m_fields;
    QString                    m_fieldValues[8];
    QDateTime                  m_lastUpdate;
    QList<TorcNetworkRequest*> m_requests;
    int                        m_badRequestCount;
};

#endif // TORCTHINGSPEAKNOTIFIER_H
