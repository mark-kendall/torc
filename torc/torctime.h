#ifndef TORCTIME_H
#define TORCTIME_H

// Qt
#include <QObject>
#include <QTimer>

// Torc
#include "http/torchttpservice.h"

class TorcTime : public QObject, public TorcHTTPService
{
    friend class TorcTimeObject;

    Q_OBJECT

    Q_CLASSINFO("Version",   "1.0.0")
    Q_PROPERTY(QString currentTime  READ GetCurrentTime  NOTIFY currentTimeChanged)

  public:
    virtual  ~TorcTime();

  protected:
    explicit  TorcTime();

  signals:
    void      currentTimeChanged (QString Time);

  public slots:
    // TorcHTTPService requirement
    void      SubscriberDeleted  (QObject *Subscriber);
    // internal timer
    void      Tick               (void);
    // Actual public slot
    QString   GetCurrentTime     (void);

  private:
    QString   currentTime; // dummy
    QDateTime m_lastTime;
    QTimer    m_timer;
    QString   m_dateTimeFormat;
};

#endif // TORCTIME_H
