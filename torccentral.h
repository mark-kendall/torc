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
    Q_OBJECT
    Q_CLASSINFO("Version",     "1.0.0")
    Q_CLASSINFO("RestartTorc", "methods=PUT")
    Q_PROPERTY(bool canRestartTorc READ GetCanRestartTorc NOTIFY CanRestartTorcChanged)

  public:
    TorcCentral();
    ~TorcCentral();

    QString         GetUIName             (void);
    bool            event                 (QEvent *Event);

  signals:
    void            CanRestartTorcChanged (bool CanRestartTorc);

  public slots:
    // TorcHTTPService
    void            SubscriberDeleted     (QObject *Subscriber);

    bool            GetCanRestartTorc     (void);
    bool            RestartTorc           (void);

  private:
    bool            LoadConfig            (void);
    QByteArray      GetCustomisedXSD      (const QString &BaseXSDFile);

  private:
    QMutex         *m_lock;
    QVariantMap     m_config;
    QByteArray     *m_graph;
    bool            canRestartTorc;
};

#define XSD_TYPES        QString("<!--TORC_XSD_TYPES-->")
#define XSD_INPUTTYPES   QString("<!--TORC_XSD_INPUTTYPES-->")
#define XSD_INPUTS       QString("<!--TORC_XSD_INPUTS-->")
#define XSD_CONTROLTYPES QString("<!--TORC_XSD_CONTROLTYPES-->")
#define XSD_CONTROLS     QString("<!--TORC_XSD_CONTROLS-->")
#define XSD_OUTPUTTYPES  QString("<!--TORC_XSD_OUTPUTTYPES-->")
#define XSD_OUTPUTS      QString("<!--TORC_XSD_OUTPUTS-->")
#define XSD_UNIQUE       QString("<!--TORC_XSD_UNIQUE-->")

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
};

#endif // TORCCENTRAL_H
