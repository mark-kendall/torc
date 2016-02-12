#ifndef TORCLOGNOTIFIER_H
#define TORCLOGNOTIFIER_H

// Torc
#include "torcnotifier.h"

class TorcLogNotifier : public TorcNotifier
{
  public:
    TorcLogNotifier(const QVariantMap &Details);
    ~TorcLogNotifier();

    void Notify (const QVariantMap &Notification);
};

#endif // TORCLOGNOTIFIER_H
