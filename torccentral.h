#ifndef TORCCENTRAL_H
#define TORCCENTRAL_H

// Qt
#include <QMutex>
#include <QObject>
#include <QVariant>

// Torc
#include "torcdevice.h"

class TorcCentral : public QObject
{
    Q_OBJECT

  public:
    TorcCentral();
    ~TorcCentral();

    bool        event          (QEvent *Event);

    static      QByteArray    *gStateGraph;
    static      QMutex        *gStateGraphLock;

  private:
    bool        LoadConfig     (void);

  private:
    QVariantMap m_config;
};
#endif // TORCCENTRAL_H
