#ifndef TORCCENTRAL_H
#define TORCCENTRAL_H

// Qt
#include <QObject>
#include <QVariant>

class TorcCentral : public QObject
{
    Q_OBJECT

  public:
    TorcCentral();
    ~TorcCentral();

    bool        event          (QEvent *Event);

  public slots:
    void        SensorsChanged (void);
    void        OutputsChanged (void);

  private:
    bool        LoadConfig     (void);
    void        LoadDevices    (void);

  private:
    QVariantMap m_config;
};

#endif // TORCCENTRAL_H
