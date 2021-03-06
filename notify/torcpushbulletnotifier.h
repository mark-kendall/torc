#ifndef TORCPUSHBULLETNOTIFIER_H
#define TORCPUSHBULLETNOTIFIER_H

// Qt
#include <QTimer>

// Torc
#include "torcnotifier.h"

class TorcNetworkRequest;

#define PUSHBULLET_PUSH_URL QStringLiteral("https://api.pushbullet.com/v2/pushes")

class TorcPushbulletNotifier final : public TorcNotifier
{
    Q_OBJECT

  public:
    explicit TorcPushbulletNotifier(const QVariantMap &Details);
    ~TorcPushbulletNotifier();

    void Notify               (const QVariantMap &Notification) override;
    QStringList GetDescription(void) override;

  signals:
    void StartResetTimer      (int MSeconds);

  public slots:
    void RequestReady         (TorcNetworkRequest* Request);
    void ResetTimerTimeout    (void);

  private:
    QTimer                     m_resetTimer;
    int                        m_networkAbort;
    QString                    m_accessToken;
    QList<TorcNetworkRequest*> m_requests;
    int                        m_badRequestCount;
};

#endif // TORCPUSHBULLETNOTIFIER_H
