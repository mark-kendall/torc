#ifndef TORCLOGNOTIFIER_H
#define TORCLOGNOTIFIER_H

// Torc
#include "torcnotifier.h"

class TorcLogNotifier Q_DECL_FINAL : public TorcNotifier
{
  public:
    explicit TorcLogNotifier(const QVariantMap &Details);
    ~TorcLogNotifier();

    void Notify (const QVariantMap &Notification) Q_DECL_OVERRIDE;
    QStringList GetDescription (void) Q_DECL_OVERRIDE;
};

#endif // TORCLOGNOTIFIER_H
