#ifndef TORCDEVICE_H
#define TORCDEVICE_H

// Qt
#include <QString>
#include <QMutex>

// Torc
#include "torcreferencecounted.h"

class TorcDevice : public TorcReferenceCounter
{
  public:
    TorcDevice(bool Valid, double Value, double Default,
               const QString &ModelId, const QString &UniqueId,
               const QString &UserName = QString(""), const QString &userDescription = QString(""));
    virtual ~TorcDevice();

  protected:
    bool                   valid;
    double                 value;
    double                 defaultValue;
    QString                modelId;
    QString                uniqueId;
    QString                userName;
    QString                userDescription;
    QMutex                *lock;

};

#endif // TORCDEVICE_H
