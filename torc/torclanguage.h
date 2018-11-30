#ifndef TORCLANGUAGE_H
#define TORCLANGUAGE_H

// Qt
#include <QMap>
#include <QObject>
#include <QLocale>
#include <QTranslator>
#include <QReadWriteLock>

// Torc
#include "torcsetting.h"
#include "torchttpservice.h"

#define DEFAULT_QT_LANGUAGE (QLocale::AnyLanguage)

class TorcLanguage : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",   "1.0.0")
    Q_PROPERTY(QString languageCode   READ GetLanguageCode   NOTIFY LanguageCodeChanged)
    Q_PROPERTY(QString languageString READ GetLanguageString NOTIFY LanguageStringChanged)

  public:
    static QMap<QString,int> gLanguageMap;
    static QString           ToString              (QLocale::Language Language, bool Empty = false);
    static QLocale::Language From2CharCode         (const char *Code);
    static QLocale::Language From2CharCode         (const QString &Code);
    static QLocale::Language From3CharCode         (const char *Code);
    static QLocale::Language From3CharCode         (const QString &Code);

  public:
    explicit TorcLanguage(TorcSetting *SettingParent);
    virtual ~TorcLanguage();

    QLocale                  GetLocale             (void);
    QString                  GetUIName             (void);

  signals:
    void                     LanguageCodeChanged   (QString Language);
    void                     LanguageStringChanged (QString String);

  public slots:
    void                     LanguageSettingChanged(const QString &Language);
    void                     SetLanguageCode       (const QString &Language);
    QString                  GetLanguageCode       (void);
    QString                  GetLanguageString     (void);
    QString                  GetTranslation        (const QString &Context, const QString &String,
                                                    const QString &Disambiguation, int Number);
    void                     SubscriberDeleted     (QObject *Subscriber);

  private:
    void                     InitialiseTranslations (void);
    static void              Initialise            (void);

  private:
    Q_DISABLE_COPY(TorcLanguage)
    TorcSetting             *m_languageSetting;
    QString                  languageCode;
    QString                  languageString;
    QLocale                  m_locale;
    QList<QLocale>           m_languages;
    QTranslator              m_translator;
};

class TorcStringFactory
{
  public:
    TorcStringFactory();
    virtual ~TorcStringFactory() = default;

    static QVariantMap           GetTorcStrings         (void);
    static TorcStringFactory*    GetTorcStringFactory   (void);
    TorcStringFactory*           NextFactory            (void) const;

    virtual void                 GetStrings             (QVariantMap &Strings) = 0;

  protected:
    static TorcStringFactory*    gTorcStringFactory;
    TorcStringFactory*           nextTorcStringFactory;

  private:
    Q_DISABLE_COPY(TorcStringFactory)
};

#endif // TORCLANGUAGE_H
