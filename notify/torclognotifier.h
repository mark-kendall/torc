#ifndef TORCLOGNOTIFIER_H
#define TORCLOGNOTIFIER_H

// Torc
#include "torcnotifier.h"

class TorcLogNotifier final : public TorcNotifier
{
    Q_OBJECT
  public:
    explicit TorcLogNotifier(const QVariantMap &Details);
    ~TorcLogNotifier() = default;

    void Notify (const QVariantMap &Notification) override;
    QStringList GetDescription (void) override;
};

#endif // TORCLOGNOTIFIER_H
