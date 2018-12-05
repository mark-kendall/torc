/* Class TorcLanguage
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QDir>
#include <QCoreApplication>

// Torc
#include "torclogging.h"
#include "torclocalcontext.h"
#include "torcdirectories.h"
#include "torclanguage.h"

QMap<QString,int> TorcLanguage::gLanguageMap;

#define BLACKLIST QStringLiteral("SetLanguageCode,LanguageSettingChanged")

/*! \class TorcLanguage
 *  \brief A class to track and manage language and locale settings and available translations.
 *
 * TorcLanguage uses the 2 or 4 character language/country code to identify a language (e.g. en_GB, en_US, en).
 * If a specific language has not been set in the settings database, the system language will be used (with a
 * final fallback to en_GB). The current language can be set with SetLanguageCode and retrieved with GetLanguageCode.
 * GetLanguageString returns a translated, user presentable string naming the language.
 *
 * GetLanguages returns a list of available translations.
 *
 * GetTranslation provides a context sensitive translation service for strings. It is intended for dynamically
 * translated strings (e.g. plurals) retrieved via the HTTP interface. Other strings should be loaded once only.
 *
 * Various utility functions are available for interfacing with 3rd party libraries (From2CharCode etc).
 *
 * TorcLanguage is available from QML via QAbstractListModel inheritance and from the HTTP interface via TorcHTTPService.
 *
 * \todo Check whether QTranslator::load is thread safe.
 * \todo Add support for multiple translation files (e.g. plugins as well ).
*/
TorcLanguage::TorcLanguage(TorcSetting *SettingParent)
  : QObject(),
    TorcHTTPService(this, QStringLiteral("languages"), QStringLiteral("languages"), TorcLanguage::staticMetaObject, BLACKLIST),
    m_languageSetting(nullptr), // Don't initialise setting until we have a local default
    languageCode(),
    languageString(),
    m_locale(),
    m_languages(),
    m_translator()
{
    QCoreApplication::installTranslator(&m_translator);

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("System language: %1 (%2) (%3)(env - %4)")
        .arg(QLocale::languageToString(m_locale.language()), QLocale::countryToString(m_locale.country()),
             m_locale.name(), QString(qgetenv(QByteArrayLiteral("LANG").constData()))));

    Initialise();

    InitialiseTranslations();
    QString language = m_locale.name(); // somewhat circular
    if (language.isEmpty())
        language = QStringLiteral("en_GB");
    m_languageSetting = new TorcSetting(SettingParent, QStringLiteral("Language"), tr("Language"), TorcSetting::String,
                                        TorcSetting::Persistent | TorcSetting::Public, QVariant(language));
    m_languageSetting->SetActive(true);
    QVariantMap selections;
    foreach (QLocale locale, m_languages)
        selections.insert(locale.name(), locale.nativeLanguageName());
    m_languageSetting->SetSelections(selections);
    SetLanguageCode(m_languageSetting->GetValue().toString());
    connect(m_languageSetting, static_cast<void (TorcSetting::*)(QString&)>(&TorcSetting::ValueChanged), this, &TorcLanguage::LanguageSettingChanged);
}

TorcLanguage::~TorcLanguage()
{
    QCoreApplication::removeTranslator(&m_translator);
    m_languageSetting->Remove();
    m_languageSetting->DownRef();
}

void TorcLanguage::LanguageSettingChanged(QString &Language)
{
    LOG(VB_GENERAL, LOG_ALERT, QStringLiteral("Language setting changed to '%1' - restarting").arg(Language));
    TorcLocalContext::NotifyEvent(Torc::RestartTorc);
}

QString TorcLanguage::GetUIName(void)
{
    return tr("Languages");
}

/*! \brief Set the current language for this application.
 *
*/
void TorcLanguage::SetLanguageCode(const QString &Language)
{
    QWriteLocker locker(&m_httpServiceLock);

    // ignore unnecessary changes
    QLocale locale(Language);
    if (m_locale == locale)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Requested language already set - ignoring"));
        return;
    }

    // ignore unknown
    if (!m_languages.contains(locale))
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Requested language (%1) not available - ignoring").arg(Language));
        return;
    }

    // set new details
    m_locale       = locale;
    languageCode   = m_locale.name();
    languageString = m_locale.nativeLanguageName();

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Language changed: %1 (%2) (%3)(env - %4)")
        .arg(QLocale::languageToString(m_locale.language()), QLocale::countryToString(m_locale.country()),
             m_locale.name(), QString(qgetenv(QByteArrayLiteral("LANG").constData()))));

    // load the new translation. This will replace the existing translation.
    // NB it's not clear from the docs whether this is thread safe.
    // NB this only supports installing a single translation file. So other 'modules' cannot
    // currently be loaded.

    QString filename = QStringLiteral("torc_%1.qm").arg(m_locale.name());
    if (!m_translator.load(filename, GetTorcTransDir()))
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to load translation file '%1' from '%2'").arg(filename, GetTorcTransDir()));

    // notify change
    emit LanguageCodeChanged(languageCode);
    emit LanguageStringChanged(languageString);
}

/// \brief Return the current language.
QString TorcLanguage::GetLanguageCode(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return languageCode;
}

QLocale TorcLanguage::GetLocale(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return m_locale;
}

QString TorcLanguage::GetLanguageString(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return languageString;
}

QString TorcLanguage::GetTranslation(const QString &Context, const QString &String, const QString &Disambiguation, int Number)
{
    return QCoreApplication::translate(Context.toUtf8().constData(), String.toUtf8().constData(),
                                       Disambiguation.toUtf8().constData(), Number);
}

void TorcLanguage::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/// \brief Return a user readable string for the current language.
QString TorcLanguage::ToString(QLocale::Language Language, bool Empty)
{
    if (Language == DEFAULT_QT_LANGUAGE)
        return Empty ? QStringLiteral("") : QStringLiteral("Unknown");

    return QLocale::languageToString(Language);
}

/// \brief Return the language associated with the given 2 character code.
QLocale::Language TorcLanguage::From2CharCode(const char *Code)
{
    return From2CharCode(QString(Code));
}

/// \brief Return the language associated with the given 2 character code.
QLocale::Language TorcLanguage::From2CharCode(const QString &Code)
{
    QString language = Code.toLower();

    if (language.size() == 2)
    {
        QLocale locale(Code);
        return locale.language();
    }

    return DEFAULT_QT_LANGUAGE;
}

/// \brief Return the language associated with the given 3 character code.
QLocale::Language TorcLanguage::From3CharCode(const char *Code)
{
    return From3CharCode(QString(Code));
}

/// \brief Return the language associated with the given 3 character code.
QLocale::Language TorcLanguage::From3CharCode(const QString &Code)
{
    QString language = Code.toLower();

    if (language.size() == 3)
    {
        QMap<QString,int>::const_iterator it = gLanguageMap.constFind(language);
        if (it != gLanguageMap.constEnd())
            return (QLocale::Language)it.value();
    }

    return DEFAULT_QT_LANGUAGE;
}

void TorcLanguage::InitialiseTranslations(void)
{
    // clear out old (just in case)
    m_languages.clear();

    // retrieve list of installed translation files
    QDir directory(GetTorcTransDir());
    QStringList files = directory.entryList(QStringList(QStringLiteral("torc_*.qm")),
                                            QDir::Files | QDir::Readable | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                                            QDir::Name);

    // create a reference list
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Found %1 translations").arg(files.size()));
    foreach (QString file, files)
    {
        file.chop(3);
        m_languages.append(QLocale(file.mid(5)));
    }
}

/*! \brief Initialise the list of supported languages.
 *
 * We aim to use the 3 character code internally for compatability with
 * other libraries and devices. To maintain performance, we create a static
 * list of languages mapping 3 character codes to Qt constants.
*/
void TorcLanguage::Initialise(void)
{
    static bool initialised = false;

    if (initialised)
        return;

    initialised = true;

    gLanguageMap.insert(QStringLiteral("aar"), QLocale::Afar);
    gLanguageMap.insert(QStringLiteral("abk"), QLocale::Abkhazian);
    gLanguageMap.insert(QStringLiteral("afr"), QLocale::Afrikaans);
    gLanguageMap.insert(QStringLiteral("aka"), QLocale::Akan);
    gLanguageMap.insert(QStringLiteral("alb"), QLocale::Albanian);
    gLanguageMap.insert(QStringLiteral("amh"), QLocale::Amharic);
    gLanguageMap.insert(QStringLiteral("ara"), QLocale::Arabic);
    gLanguageMap.insert(QStringLiteral("arm"), QLocale::Armenian);
    gLanguageMap.insert(QStringLiteral("asm"), QLocale::Assamese);
    gLanguageMap.insert(QStringLiteral("aym"), QLocale::Aymara);
    gLanguageMap.insert(QStringLiteral("aze"), QLocale::Azerbaijani);
    gLanguageMap.insert(QStringLiteral("bak"), QLocale::Bashkir);
    gLanguageMap.insert(QStringLiteral("bam"), QLocale::Bambara);
    gLanguageMap.insert(QStringLiteral("baq"), QLocale::Basque);
    gLanguageMap.insert(QStringLiteral("bel"), QLocale::Byelorussian);
    gLanguageMap.insert(QStringLiteral("ben"), QLocale::Bengali);
    gLanguageMap.insert(QStringLiteral("bih"), QLocale::Bihari);
    gLanguageMap.insert(QStringLiteral("bis"), QLocale::Bislama);
    gLanguageMap.insert(QStringLiteral("bos"), QLocale::Bosnian);
    gLanguageMap.insert(QStringLiteral("bre"), QLocale::Breton);
    gLanguageMap.insert(QStringLiteral("bul"), QLocale::Bulgarian);
    gLanguageMap.insert(QStringLiteral("bur"), QLocale::Burmese);
    gLanguageMap.insert(QStringLiteral("byn"), QLocale::Blin);
    gLanguageMap.insert(QStringLiteral("cat"), QLocale::Catalan);
    gLanguageMap.insert(QStringLiteral("cch"), QLocale::Atsam);
    gLanguageMap.insert(QStringLiteral("chi"), QLocale::Chinese);
    gLanguageMap.insert(QStringLiteral("cor"), QLocale::Cornish);
    gLanguageMap.insert(QStringLiteral("cos"), QLocale::Corsican);
    gLanguageMap.insert(QStringLiteral("cze"), QLocale::Czech);
    gLanguageMap.insert(QStringLiteral("dan"), QLocale::Danish);
    gLanguageMap.insert(QStringLiteral("div"), QLocale::Divehi);
    gLanguageMap.insert(QStringLiteral("dut"), QLocale::Dutch);
    gLanguageMap.insert(QStringLiteral("dzo"), QLocale::Bhutani);
    gLanguageMap.insert(QStringLiteral("eng"), QLocale::English);
    gLanguageMap.insert(QStringLiteral("epo"), QLocale::Esperanto);
    gLanguageMap.insert(QStringLiteral("est"), QLocale::Estonian);
    gLanguageMap.insert(QStringLiteral("ewe"), QLocale::Ewe);
    gLanguageMap.insert(QStringLiteral("fao"), QLocale::Faroese);
    gLanguageMap.insert(QStringLiteral("fil"), QLocale::Filipino);
    gLanguageMap.insert(QStringLiteral("fin"), QLocale::Finnish);
    gLanguageMap.insert(QStringLiteral("fre"), QLocale::French);
    gLanguageMap.insert(QStringLiteral("fry"), QLocale::Frisian);
    gLanguageMap.insert(QStringLiteral("ful"), QLocale::Fulah);
    gLanguageMap.insert(QStringLiteral("fur"), QLocale::Friulian);
    gLanguageMap.insert(QStringLiteral("gaa"), QLocale::Ga);
    gLanguageMap.insert(QStringLiteral("geo"), QLocale::Georgian);
    gLanguageMap.insert(QStringLiteral("ger"), QLocale::German);
    gLanguageMap.insert(QStringLiteral("gez"), QLocale::Geez);
    gLanguageMap.insert(QStringLiteral("gla"), QLocale::Gaelic);
    gLanguageMap.insert(QStringLiteral("gle"), QLocale::Irish);
    gLanguageMap.insert(QStringLiteral("glg"), QLocale::Galician);
    gLanguageMap.insert(QStringLiteral("glv"), QLocale::Manx);
    gLanguageMap.insert(QStringLiteral("gre"), QLocale::Greek);
    gLanguageMap.insert(QStringLiteral("grn"), QLocale::Guarani);
    gLanguageMap.insert(QStringLiteral("guj"), QLocale::Gujarati);
    gLanguageMap.insert(QStringLiteral("hau"), QLocale::Hausa);
    gLanguageMap.insert(QStringLiteral("haw"), QLocale::Hawaiian);
    gLanguageMap.insert(QStringLiteral("hbs"), QLocale::SerboCroatian);
    gLanguageMap.insert(QStringLiteral("heb"), QLocale::Hebrew);
    gLanguageMap.insert(QStringLiteral("hin"), QLocale::Hindi);
    gLanguageMap.insert(QStringLiteral("hun"), QLocale::Hungarian);
    gLanguageMap.insert(QStringLiteral("ibo"), QLocale::Igbo);
    gLanguageMap.insert(QStringLiteral("ice"), QLocale::Icelandic);
    gLanguageMap.insert(QStringLiteral("iii"), QLocale::SichuanYi);
    gLanguageMap.insert(QStringLiteral("iku"), QLocale::Inuktitut);
    gLanguageMap.insert(QStringLiteral("ile"), QLocale::Interlingue);
    gLanguageMap.insert(QStringLiteral("ina"), QLocale::Interlingua);
    gLanguageMap.insert(QStringLiteral("ind"), QLocale::Indonesian);
    gLanguageMap.insert(QStringLiteral("ipk"), QLocale::Inupiak);
    gLanguageMap.insert(QStringLiteral("ita"), QLocale::Italian);
    gLanguageMap.insert(QStringLiteral("jav"), QLocale::Javanese);
    gLanguageMap.insert(QStringLiteral("jpn"), QLocale::Japanese);
    gLanguageMap.insert(QStringLiteral("kaj"), QLocale::Jju);
    gLanguageMap.insert(QStringLiteral("kal"), QLocale::Greenlandic);
    gLanguageMap.insert(QStringLiteral("kan"), QLocale::Kannada);
    gLanguageMap.insert(QStringLiteral("kam"), QLocale::Kamba);
    gLanguageMap.insert(QStringLiteral("kas"), QLocale::Kashmiri);
    gLanguageMap.insert(QStringLiteral("kaz"), QLocale::Kazakh);
    gLanguageMap.insert(QStringLiteral("kcg"), QLocale::Tyap);
    gLanguageMap.insert(QStringLiteral("kfo"), QLocale::Koro);
    gLanguageMap.insert(QStringLiteral("khm"), QLocale::Cambodian);
    gLanguageMap.insert(QStringLiteral("kik"), QLocale::Kikuyu);
    gLanguageMap.insert(QStringLiteral("kin"), QLocale::Kinyarwanda);
    gLanguageMap.insert(QStringLiteral("kir"), QLocale::Kirghiz);
    gLanguageMap.insert(QStringLiteral("kok"), QLocale::Konkani);
    gLanguageMap.insert(QStringLiteral("kor"), QLocale::Korean);
    gLanguageMap.insert(QStringLiteral("kpe"), QLocale::Kpelle);
    gLanguageMap.insert(QStringLiteral("kur"), QLocale::Kurdish);
    gLanguageMap.insert(QStringLiteral("lat"), QLocale::Latin);
    gLanguageMap.insert(QStringLiteral("lav"), QLocale::Latvian);
    gLanguageMap.insert(QStringLiteral("lin"), QLocale::Lingala);
    gLanguageMap.insert(QStringLiteral("lit"), QLocale::Lithuanian);
    gLanguageMap.insert(QStringLiteral("lug"), QLocale::Ganda);
    gLanguageMap.insert(QStringLiteral("mac"), QLocale::Macedonian);
    gLanguageMap.insert(QStringLiteral("mal"), QLocale::Malayalam);
    gLanguageMap.insert(QStringLiteral("mao"), QLocale::Maori);
    gLanguageMap.insert(QStringLiteral("mar"), QLocale::Marathi);
    gLanguageMap.insert(QStringLiteral("may"), QLocale::Malay);
    gLanguageMap.insert(QStringLiteral("mlg"), QLocale::Malagasy);
    gLanguageMap.insert(QStringLiteral("mlt"), QLocale::Maltese);
    gLanguageMap.insert(QStringLiteral("mol"), QLocale::Moldavian);
    gLanguageMap.insert(QStringLiteral("mon"), QLocale::Mongolian);
    gLanguageMap.insert(QStringLiteral("nau"), QLocale::NauruLanguage);
    gLanguageMap.insert(QStringLiteral("nbl"), QLocale::SouthNdebele);
    gLanguageMap.insert(QStringLiteral("nde"), QLocale::NorthNdebele);
    gLanguageMap.insert(QStringLiteral("nds"), QLocale::LowGerman);
    gLanguageMap.insert(QStringLiteral("nep"), QLocale::Nepali);
    gLanguageMap.insert(QStringLiteral("nno"), QLocale::NorwegianNynorsk);
    gLanguageMap.insert(QStringLiteral("nob"), QLocale::NorwegianBokmal);
    gLanguageMap.insert(QStringLiteral("nor"), QLocale::Norwegian);
    gLanguageMap.insert(QStringLiteral("nya"), QLocale::Chewa);
    gLanguageMap.insert(QStringLiteral("oci"), QLocale::Occitan);
    gLanguageMap.insert(QStringLiteral("ori"), QLocale::Oriya);
    gLanguageMap.insert(QStringLiteral("orm"), QLocale::Afan);
    gLanguageMap.insert(QStringLiteral("pan"), QLocale::Punjabi);
    gLanguageMap.insert(QStringLiteral("per"), QLocale::Persian);
    gLanguageMap.insert(QStringLiteral("pol"), QLocale::Polish);
    gLanguageMap.insert(QStringLiteral("por"), QLocale::Portuguese);
    gLanguageMap.insert(QStringLiteral("pus"), QLocale::Pashto);
    gLanguageMap.insert(QStringLiteral("que"), QLocale::Quechua);
    gLanguageMap.insert(QStringLiteral("roh"), QLocale::RhaetoRomance);
    gLanguageMap.insert(QStringLiteral("rum"), QLocale::Romanian);
    gLanguageMap.insert(QStringLiteral("run"), QLocale::Kurundi);
    gLanguageMap.insert(QStringLiteral("rus"), QLocale::Russian);
    gLanguageMap.insert(QStringLiteral("san"), QLocale::Sanskrit);
    gLanguageMap.insert(QStringLiteral("scc"), QLocale::Serbian);
    gLanguageMap.insert(QStringLiteral("scr"), QLocale::Croatian);
    gLanguageMap.insert(QStringLiteral("sid"), QLocale::Sidamo);
    gLanguageMap.insert(QStringLiteral("slo"), QLocale::Slovak);
    gLanguageMap.insert(QStringLiteral("slv"), QLocale::Slovenian);
    gLanguageMap.insert(QStringLiteral("syr"), QLocale::Syriac);
    gLanguageMap.insert(QStringLiteral("sme"), QLocale::NorthernSami);
    gLanguageMap.insert(QStringLiteral("smo"), QLocale::Samoan);
    gLanguageMap.insert(QStringLiteral("sna"), QLocale::Shona);
    gLanguageMap.insert(QStringLiteral("snd"), QLocale::Sindhi);
    gLanguageMap.insert(QStringLiteral("som"), QLocale::Somali);
    gLanguageMap.insert(QStringLiteral("spa"), QLocale::Spanish);
    gLanguageMap.insert(QStringLiteral("sun"), QLocale::Sundanese);
    gLanguageMap.insert(QStringLiteral("swa"), QLocale::Swahili);
    gLanguageMap.insert(QStringLiteral("swe"), QLocale::Swedish);
    gLanguageMap.insert(QStringLiteral("tam"), QLocale::Tamil);
    gLanguageMap.insert(QStringLiteral("tat"), QLocale::Tatar);
    gLanguageMap.insert(QStringLiteral("tel"), QLocale::Telugu);
    gLanguageMap.insert(QStringLiteral("tgk"), QLocale::Tajik);
    gLanguageMap.insert(QStringLiteral("tgl"), QLocale::Tagalog);
    gLanguageMap.insert(QStringLiteral("tha"), QLocale::Thai);
    gLanguageMap.insert(QStringLiteral("tib"), QLocale::Tibetan);
    gLanguageMap.insert(QStringLiteral("tig"), QLocale::Tigre);
    gLanguageMap.insert(QStringLiteral("tir"), QLocale::Tigrinya);
    gLanguageMap.insert(QStringLiteral("tso"), QLocale::Tsonga);
    gLanguageMap.insert(QStringLiteral("tuk"), QLocale::Turkmen);
    gLanguageMap.insert(QStringLiteral("tur"), QLocale::Turkish);
    gLanguageMap.insert(QStringLiteral("twi"), QLocale::Twi);
    gLanguageMap.insert(QStringLiteral("uig"), QLocale::Uigur);
    gLanguageMap.insert(QStringLiteral("ukr"), QLocale::Ukrainian);
    gLanguageMap.insert(QStringLiteral("urd"), QLocale::Urdu);
    gLanguageMap.insert(QStringLiteral("uzb"), QLocale::Uzbek);
    gLanguageMap.insert(QStringLiteral("ven"), QLocale::Venda);
    gLanguageMap.insert(QStringLiteral("vie"), QLocale::Vietnamese);
    gLanguageMap.insert(QStringLiteral("vol"), QLocale::Volapuk);
    gLanguageMap.insert(QStringLiteral("wal"), QLocale::Walamo);
    gLanguageMap.insert(QStringLiteral("wel"), QLocale::Welsh);
    gLanguageMap.insert(QStringLiteral("wol"), QLocale::Wolof);
    gLanguageMap.insert(QStringLiteral("xho"), QLocale::Xhosa);
    gLanguageMap.insert(QStringLiteral("yid"), QLocale::Yiddish);
    gLanguageMap.insert(QStringLiteral("yor"), QLocale::Yoruba);
    gLanguageMap.insert(QStringLiteral("zha"), QLocale::Zhuang);
    gLanguageMap.insert(QStringLiteral("zul"), QLocale::Zulu);

    gLanguageMap.insert(QStringLiteral("sqi"), QLocale::Albanian);
    gLanguageMap.insert(QStringLiteral("hye"), QLocale::Armenian);
    gLanguageMap.insert(QStringLiteral("eus"), QLocale::Basque);
    gLanguageMap.insert(QStringLiteral("mya"), QLocale::Burmese);
    gLanguageMap.insert(QStringLiteral("zho"), QLocale::Chinese);
    gLanguageMap.insert(QStringLiteral("ces"), QLocale::Czech);
    gLanguageMap.insert(QStringLiteral("nld"), QLocale::Dutch);
    gLanguageMap.insert(QStringLiteral("fra"), QLocale::French);
    gLanguageMap.insert(QStringLiteral("kat"), QLocale::Georgian);
    gLanguageMap.insert(QStringLiteral("deu"), QLocale::German);
    gLanguageMap.insert(QStringLiteral("ell"), QLocale::Greek);
    gLanguageMap.insert(QStringLiteral("isl"), QLocale::Icelandic);
    gLanguageMap.insert(QStringLiteral("mkd"), QLocale::Macedonian);
    gLanguageMap.insert(QStringLiteral("mri"), QLocale::Maori);
    gLanguageMap.insert(QStringLiteral("msa"), QLocale::Malay);
    gLanguageMap.insert(QStringLiteral("fas"), QLocale::Persian);
    gLanguageMap.insert(QStringLiteral("ron"), QLocale::Romanian);
    gLanguageMap.insert(QStringLiteral("srp"), QLocale::Serbian);
    gLanguageMap.insert(QStringLiteral("hrv"), QLocale::Croatian);
    gLanguageMap.insert(QStringLiteral("slk"), QLocale::Slovak);
    gLanguageMap.insert(QStringLiteral("bod"), QLocale::Tibetan);
    gLanguageMap.insert(QStringLiteral("cym"), QLocale::Welsh);
    gLanguageMap.insert(QStringLiteral("chs"), QLocale::Chinese);

    gLanguageMap.insert(QStringLiteral("nso"), QLocale::NorthernSotho);
    gLanguageMap.insert(QStringLiteral("trv"), QLocale::Taroko);
    gLanguageMap.insert(QStringLiteral("guz"), QLocale::Gusii);
    gLanguageMap.insert(QStringLiteral("dav"), QLocale::Taita);
    gLanguageMap.insert(QStringLiteral("saq"), QLocale::Samburu);
    gLanguageMap.insert(QStringLiteral("seh"), QLocale::Sena);
    gLanguageMap.insert(QStringLiteral("rof"), QLocale::Rombo);
    gLanguageMap.insert(QStringLiteral("shi"), QLocale::Tachelhit);
    gLanguageMap.insert(QStringLiteral("kab"), QLocale::Kabyle);
    gLanguageMap.insert(QStringLiteral("nyn"), QLocale::Nyankole);
    gLanguageMap.insert(QStringLiteral("bez"), QLocale::Bena);
    gLanguageMap.insert(QStringLiteral("vun"), QLocale::Vunjo);
    gLanguageMap.insert(QStringLiteral("ebu"), QLocale::Embu);
    gLanguageMap.insert(QStringLiteral("chr"), QLocale::Cherokee);
    gLanguageMap.insert(QStringLiteral("mfe"), QLocale::Morisyen);
    gLanguageMap.insert(QStringLiteral("kde"), QLocale::Makonde);
    gLanguageMap.insert(QStringLiteral("lag"), QLocale::Langi);
    gLanguageMap.insert(QStringLiteral("bem"), QLocale::Bemba);
    gLanguageMap.insert(QStringLiteral("kea"), QLocale::Kabuverdianu);
    gLanguageMap.insert(QStringLiteral("mer"), QLocale::Meru);
    gLanguageMap.insert(QStringLiteral("kln"), QLocale::Kalenjin);
    gLanguageMap.insert(QStringLiteral("naq"), QLocale::Nama);
    gLanguageMap.insert(QStringLiteral("jmc"), QLocale::Machame);
    gLanguageMap.insert(QStringLiteral("ksh"), QLocale::Colognian);
    gLanguageMap.insert(QStringLiteral("mas"), QLocale::Masai);
    gLanguageMap.insert(QStringLiteral("xog"), QLocale::Soga);
    gLanguageMap.insert(QStringLiteral("luy"), QLocale::Luyia);
    gLanguageMap.insert(QStringLiteral("asa"), QLocale::Asu);
    gLanguageMap.insert(QStringLiteral("teo"), QLocale::Teso);
    gLanguageMap.insert(QStringLiteral("ssy"), QLocale::Saho);
    gLanguageMap.insert(QStringLiteral("khq"), QLocale::KoyraChiini);
    gLanguageMap.insert(QStringLiteral("rwk"), QLocale::Rwa);
    gLanguageMap.insert(QStringLiteral("luo"), QLocale::Luo);
    gLanguageMap.insert(QStringLiteral("cgg"), QLocale::Chiga);
    gLanguageMap.insert(QStringLiteral("tzm"), QLocale::CentralMoroccoTamazight);
    gLanguageMap.insert(QStringLiteral("ses"), QLocale::KoyraboroSenni);
    gLanguageMap.insert(QStringLiteral("ksb"), QLocale::Shambala);
    gLanguageMap.insert(QStringLiteral("gsw"), QLocale::SwissGerman);
    gLanguageMap.insert(QStringLiteral("fij"), QLocale::Fijian);
    gLanguageMap.insert(QStringLiteral("lao"), QLocale::Lao);
    gLanguageMap.insert(QStringLiteral("sag"), QLocale::Sango);
    gLanguageMap.insert(QStringLiteral("sin"), QLocale::Sinhala);
    gLanguageMap.insert(QStringLiteral("sot"), QLocale::SouthernSotho);
    gLanguageMap.insert(QStringLiteral("ssw"), QLocale::Swati);
    gLanguageMap.insert(QStringLiteral("ton"), QLocale::Tongan);
    gLanguageMap.insert(QStringLiteral("tsn"), QLocale::Tswana);
}

/*! \class TorcStringFactory
 *  \brief A factory class to register translatable strings for use with external interfaces/applications.
 *
 * A translatable string is registered with a string constant that should uniquely identify it. The list
 * of registered constants and their *current* translations can be retrieved with GetTorcStrings.
 *
 * Objects that wish to register strings should sub-class TorcStringFactory and implement GetStrings.
 *
 * The string list is made available to web interfaces via the dynamically generated torcconfiguration.js file
 * and is exported directly to all QML contexts.
*/
TorcStringFactory* TorcStringFactory::gTorcStringFactory = nullptr;

TorcStringFactory::TorcStringFactory()
  : nextTorcStringFactory(gTorcStringFactory)
{
    gTorcStringFactory = this;
}

TorcStringFactory* TorcStringFactory::GetTorcStringFactory(void)
{
    return gTorcStringFactory;
}

TorcStringFactory* TorcStringFactory::NextFactory(void) const
{
    return nextTorcStringFactory;
}

/*! \brief Return a map of string constants and their translations.
*/
QVariantMap TorcStringFactory::GetTorcStrings(void)
{
    QVariantMap strings;
    TorcStringFactory* factory = TorcStringFactory::GetTorcStringFactory();
    for ( ; factory; factory = factory->NextFactory())
        factory->GetStrings(strings);
    return strings;
}
