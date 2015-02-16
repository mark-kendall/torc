#ifndef TORCDEVICE_H
#define TORCDEVICE_H

// Qt
#include <QHash>
#include <QMutex>
#include <QString>

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

  public:
    static QHash<QString,void*> *gDeviceList;
    static QMutex         *gDeviceListLock;
    static bool            UniqueIdAvailable  (const QString &UniqueId);
    static bool            RegisterUniqueId   (const QString &UniqueId, void *Object);
    static void            UnregisterUniqueId (const QString &UniqueId);
    static void*           GetObjectforId     (const QString &UniqueId);
};

#endif // TORCDEVICE_H
