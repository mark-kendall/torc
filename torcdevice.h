#ifndef TORCDEVICE_H
#define TORCDEVICE_H

// Qt
#include <QHash>
#include <QMutex>
#include <QString>
#include <QObject>

// Torc
#include "torcreferencecounted.h"

class TorcDevice : public QObject, public TorcReferenceCounter
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
    static QHash<QString,QObject*> *gDeviceList;
    static QMutex         *gDeviceListLock;
    static bool            UniqueIdAvailable  (const QString &UniqueId);
    static bool            RegisterUniqueId   (const QString &UniqueId, QObject *Object);
    static void            UnregisterUniqueId (const QString &UniqueId);
    static QObject*        GetObjectforId     (const QString &UniqueId);
};

#endif // TORCDEVICE_H
