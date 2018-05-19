#ifndef TORCCENTRAL_H
#define TORCCENTRAL_H

// Qt
#include <QMutex>
#include <QObject>
#include <QVariant>

// Torc
#include "torcdevice.h"
#include "http/torchttpservice.h"

class TorcCentral : public QObject, public TorcHTTPService
{
  public:
    enum TemperatureUnits
    {
        Celsius = 0,
        Fahrenheit = 1
    };
    Q_ENUMS(TemperatureUnits)

    Q_OBJECT
    Q_CLASSINFO("Version",     "1.0.0")
    Q_PROPERTY(QString temperatureUnits READ GetTemperatureUnits CONSTANT)

  public:
    TorcCentral();
    ~TorcCentral();

    QString         GetUIName             (void);
    bool            event                 (QEvent *Event);
    static QByteArray GetCustomisedXSD    (const QString &BaseXSDFile);
    static TemperatureUnits GetGlobalTemperatureUnits (void);
    static QString   TemperatureUnitsToString(TemperatureUnits Units);

  public slots:
    // TorcHTTPService
    void            SubscriberDeleted     (QObject *Subscriber);

    QString         GetTemperatureUnits   (void) const;

  protected:
    static TemperatureUnits gTemperatureUnits;

  private:
    bool            LoadConfig            (void);

  private:
    QMutex          m_lock;
    QVariantMap     m_config;
    QByteArray      m_graph;

    QString         temperatureUnits;
};

#define XSD_TYPES             QString("<!--TORC_XSD_TYPES-->")
#define XSD_INPUTTYPES        QString("<!--TORC_XSD_INPUTTYPES-->")
#define XSD_INPUTS            QString("<!--TORC_XSD_INPUTS-->")
#define XSD_CONTROLTYPES      QString("<!--TORC_XSD_CONTROLTYPES-->")
#define XSD_CONTROLS          QString("<!--TORC_XSD_CONTROLS-->")
#define XSD_OUTPUTTYPES       QString("<!--TORC_XSD_OUTPUTTYPES-->")
#define XSD_OUTPUTS           QString("<!--TORC_XSD_OUTPUTS-->")
#define XSD_NOTIFIERTYPES     QString("<!--TORC_XSD_NOTIFIERTYPES-->")
#define XSD_NOTIFIERS         QString("<!--TORC_XSD_NOTIFIERS-->")
#define XSD_NOTIFICATIONTYPES QString("<!--TORC_XSD_NOTIFICATIONYPES-->")
#define XSD_NOTIFICATIONS     QString("<!--TORC_XSD_NOTIFICATIONS-->")
#define XSD_UNIQUE            QString("<!--TORC_XSD_UNIQUE-->")

class TorcXSDFactory
{
  public:
    TorcXSDFactory();
    virtual ~TorcXSDFactory();

    static void               CustomiseXSD        (QByteArray &XSD);
    static TorcXSDFactory*    GetTorcXSDFactory   (void);
    TorcXSDFactory*           NextFactory         (void) const;

    virtual void              GetXSD              (QMultiMap<QString,QString> &XSD) = 0;

  protected:
    static TorcXSDFactory*    gTorcXSDFactory;
    TorcXSDFactory*           nextTorcXSDFactory;

  private:
    Q_DISABLE_COPY(TorcXSDFactory)
};

#endif // TORCCENTRAL_H
