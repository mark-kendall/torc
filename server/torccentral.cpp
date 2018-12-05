/* Class TorcCentral
*
* Copyright (C) Mark Kendall 2015-18
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>

// Torc
#include "torcexitcodes.h"
#include "torccoreutils.h"
#include "torcevent.h"
#include "torclogging.h"
#include "torcdirectories.h"
#include "torclanguage.h"
#include "torclocalcontext.h"
#include "torc/torcadminthread.h"
#include "inputs/torcinputs.h"
#include "outputs/torcoutputs.h"
#include "controls/torccontrols.h"
#include "notify/torcnotify.h"
#include "torcxmlreader.h"
#include "torccentral.h"

#ifdef USING_XMLPATTERNS
#include "torcxmlvalidator.h"
#elif USING_LIBXML2
#include "torclibxmlvalidator.h"
#endif

#ifdef USING_GRAPHVIZ_LIBS
#include <graphviz/gvc.h>
#endif

// for system
#include <stdlib.h>

// default to metric temperature measurements
TorcCentral::TemperatureUnits TorcCentral::gTemperatureUnits = TorcCentral::Celsius;

TorcCentral::TemperatureUnits TorcCentral::GetGlobalTemperatureUnits(void)
{
    return gTemperatureUnits;
}

TorcCentral::TorcCentral()
  : QObject(),
    TorcHTTPService(this, QStringLiteral("central"), QStringLiteral("central"), TorcCentral::staticMetaObject, QStringLiteral("")),
    m_config(QVariantMap()),
    m_graph(),
    temperatureUnits(QStringLiteral("celsius"))
{
    // reset state graph and clear out old files
    // content directory should already have been created by TorcHTMLDynamicContent
    QString graphdot = QStringLiteral("%1stategraph.dot").arg(GetTorcContentDir());
    QString graphsvg = QStringLiteral("%1stategraph.svg").arg(GetTorcContentDir());
    QString config   = QStringLiteral("%1%2").arg(GetTorcContentDir(), TORC_CONFIG_FILE);
    QString current  = QStringLiteral("%1/%2").arg(GetTorcConfigDir(), TORC_CONFIG_FILE);

    if (QFile::exists(graphdot))
        QFile::remove(graphdot);
    if (QFile::exists(graphsvg))
        QFile::remove(graphsvg);
    if (QFile::exists(config))
        QFile::remove(config);

    QFile currentconfig(current);
    if (!currentconfig.copy(config))
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to copy current config file to content directory"));

    // listen for interesting events
    gLocalContext->AddObserver(this);

    if (LoadConfig())
    {
        TemperatureUnits temperatureunits = Celsius; // default to metric

        // handle settings now
        if (m_config.contains(QStringLiteral("settings")))
        {
            QVariantMap settings = m_config.value(QStringLiteral("settings")).toMap();

            // applicationname
            if (settings.contains(QStringLiteral("applicationname")))
            {
                QString name = settings.value(QStringLiteral("applicationname")).toString().trimmed();
                if (!name.isEmpty())
                {
                    QCoreApplication::setApplicationName(name);
                    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Changed application name to '%1'").arg(name));
                }
            }

            // temperature units - metric or imperial...
            if (settings.contains(QStringLiteral("temperatureunits")))
            {
                QString units = settings.value(QStringLiteral("temperatureunits")).toString().trimmed().toLower();
                if (units == QStringLiteral("metric") || units == QStringLiteral("celsius"))
                    temperatureunits = Celsius;
                else if (units == QStringLiteral("imperial") || units == QStringLiteral("fahrenheit"))
                    temperatureunits = Fahrenheit;
                else
                    LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Unknown temperature units - defaulting to metric (Celsius)"));
            }
        }

        TorcCentral::gTemperatureUnits = temperatureunits;
        temperatureUnits               = TorcCoreUtils::EnumToLowerString<TorcCentral::TemperatureUnits>(temperatureunits);
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Using '%1' temperature units").arg(temperatureUnits));

        // create the devices
        TorcDeviceHandler::Start(m_config);

        // connect controls to sensors/outputs/other controls
        TorcControls::gControls->Validate();

        // setup notifications/notifiers
        (void)TorcNotify::gNotify->Validate();

        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Initialising state machine"));
        // initialise the state machine
        {
            QMutexLocker lock(TorcDevice::gDeviceListLock);

            QHash<QString,TorcDevice*>::const_iterator it = TorcDevice::gDeviceList->constBegin();
            for( ; it != TorcDevice::gDeviceList->constEnd(); ++it)
                it.value()->Start();
        }

        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Notifying start"));
        TorcLocalContext::NotifyEvent(Torc::Start);

        // iff we have got this far, then create the graph
        // start the graph
        m_graph.clear();
        m_graph.append(QStringLiteral("strict digraph \"%1\" {\r\n"
                                    "    rankdir=\"LR\";\r\n"
                                    "    node [shape=rect];\r\n")
                            .arg(TORC_TORC));

        // build the graph contents
        TorcInputs::gInputs->Graph(&m_graph);
        TorcOutputs::gOutputs->Graph(&m_graph);
        TorcControls::gControls->Graph(&m_graph);
        TorcNotify::gNotify->Graph(&m_graph);

        // complete the graph
        m_graph.append("}\r\n");

        QFile file(graphdot);
        if (file.open(QIODevice::ReadWrite))
        {
            file.write(m_graph);
            file.flush();
            file.close();
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Saved state graph as %1").arg(graphdot));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to open '%1' to write state graph").arg(graphdot));
        }

        // create a representation of the state graph
#ifdef USING_GRAPHVIZ_LIBS
        bool created = false;

        FILE *handle = fopen(graphsvg.toLocal8Bit().data(), "w");
        if (handle)
        {
            GVC_t *gvc  = gvContext();
            Agraph_t *g = agmemread(m_graph.data());
            gvLayout(gvc, g, "dot");
            gvRender(gvc, g, "svg", handle);
            gvFreeLayout(gvc,g);
            agclose(g);
            gvFreeContext(gvc);
            fclose(handle);
            created = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to open '%1' for writing (err: %2)").arg(graphsvg, strerror(errno)));
        }

        if (!created)
        {
#endif
            // NB QProcess appears to be fatally broken. Just use system instead
            QString command = QStringLiteral("dot -Tsvg -o %1 %2").arg(graphsvg, graphdot);
            int err = system(command.toLocal8Bit());
            if (err < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create stategraph representation (err: %1)").arg(strerror(errno)));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Saved state graph representation as %1").arg(graphsvg));
                if (err > 0)
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("dot returned an unexpected result - stategraph may be incomplete or absent"));
            }
        }
#ifdef USING_GRAPHVIZ_LIBS
    }
#endif
    // no need for the graph data from here
    m_graph.clear();
}

TorcCentral::~TorcCentral()
{
    // deregister for events
    gLocalContext->RemoveObserver(this);

    // cleanup any devices
    TorcDeviceHandler::Stop();
}

QString TorcCentral::GetUIName(void)
{
    return tr("Central");
}

QString TorcCentral::GetTemperatureUnits(void)
{
    return temperatureUnits;
}

void TorcCentral::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

bool TorcCentral::LoadConfig(void)
{
    bool skipvalidation = false;

    if (!qEnvironmentVariableIsEmpty("TORC_NO_VALIDATION"))
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Skipping configuration file validation (command line)."));
        skipvalidation = true;
    }

    QString xml = GetTorcConfigDir() + "/" + TORC_CONFIG_FILE;
    QFileInfo config(xml);
    if (!skipvalidation && !config.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to find configuration file '%1'").arg(xml));
        return false;
    }

#if defined(USING_XMLPATTERNS) || defined(USING_LIBXML2)
    QString customxsd = GetTorcContentDir() + "torc.xsd";
    // we always want to delete the old xsd - if it isn't present, it wasn't used!
    // so retrieve now and then delete
    QByteArray oldxsd;
    if (QFile::exists(customxsd))
    {
        QFile customxsdfile(customxsd);
        if (customxsdfile.open(QIODevice::ReadOnly))
        {
            oldxsd = customxsdfile.readAll();
            customxsdfile.close();
        }
        QFile::remove(customxsd);
    }

    QString basexsd = GetTorcShareDir() + "/html/torc.xsd";
    if (!QFile::exists(basexsd))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to find base XSD file '%1'").arg(basexsd));
        return false;
    }

    QByteArray newxsd;
    if (!skipvalidation)
    {
        // customise and save the xsd now - even if validation is skipped from here, we need the
        // xsd for reference
        newxsd = GetCustomisedXSD(basexsd);
        if (!newxsd.isEmpty())
        {
            // save the XSD for the user to inspect if necessary
            QFile customxsdfile(customxsd);
            if (customxsdfile.open(QIODevice::ReadWrite))
            {
                customxsdfile.write(newxsd);
                customxsdfile.flush();
                customxsdfile.close();
                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Saved current XSD as '%1'").arg(customxsd));
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to open '%1' for writing").arg(customxsd));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Strange - empty xsd..."));
            return false;
        }
    }

    // validation can take a while on slower machines (e.g. single core raspberry pi).
    // try and skip if the config has not been modified
    QString lastvalidated = gLocalContext->GetSetting(QStringLiteral("configLastValidated"), QStringLiteral("never"));
    if (!skipvalidation && lastvalidated != QStringLiteral("never"))
    {
        bool xsdmodified    = qstrcmp(oldxsd.constData(), newxsd.constData()) != 0;
        bool configmodified = config.lastModified() >= QDateTime::fromString(lastvalidated);

        if (xsdmodified)
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("XSD file changed since last validation"));
        else
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("XSD unchanged since last validation"));

        if (configmodified)
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Configuration file changed since last validation"));
        else
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Configuration file unchanged since last validation:"));

        skipvalidation = !xsdmodified && !configmodified;
    }

    if (!skipvalidation)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Starting validation of configuration file"));
        TorcXmlValidator validator(xml, newxsd);
        if (!validator.Validated())
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Configuration file '%1' failed validation").arg(xml));
            // make sure we re-validate
            gLocalContext->SetSetting(QStringLiteral("configLastValidated"), QStringLiteral("never"));
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Configuration successfully validated"));
        gLocalContext->SetSetting(QStringLiteral("configLastValidated"), QDateTime::currentDateTime().toString());
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Skipping validation of configuration file"));
    }
#else
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Xml validation unavailable - not validating configuration file."));
#endif

    TorcXMLReader reader(xml);
    QString error;

    if (!reader.IsValid(error))
    {
        LOG(VB_GENERAL, LOG_ERR, error);
        return false;
    }

    QVariantMap result = reader.GetResult();

    // root object should be 'torc'
    if (!result.contains(QStringLiteral("torc")))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to find 'torc' root element in '%1'").arg(xml));
        return false;
    }

    m_config = result.value(QStringLiteral("torc")).toMap();

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Loaded config from %1").arg(xml));
    return true;
}

QByteArray TorcCentral::GetCustomisedXSD(const QString &BaseXSDFile)
{
    QByteArray result;
    if (!QFile::exists(BaseXSDFile))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Base XSD file '%1' does not exist").arg(BaseXSDFile));
        return result;
    }

    QFile xsd(BaseXSDFile);
    if (!xsd.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to open base XSD file '%1'").arg(BaseXSDFile));
        return result;
    }

    result = xsd.readAll();
    xsd.close();
    TorcXSDFactory::CustomiseXSD(result);
    return result;
}

/// Handle Exit events
bool TorcCentral::event(QEvent *Event)
{
    TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
    if (torcevent)
    {
        int event = torcevent->GetEvent();
        switch (event)
        {
            case Torc::ShuttingDown:
            case Torc::Suspending:
            case Torc::Restarting:
            case Torc::Hibernating:
                // NB this just ensures outputs (e.g. PWM drivers) are set to sensible
                // defaults if we shut down. There is currently no handling of waking, which
                // would need more thought about resetting state etc.
                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Resetting devices to defaults."));
                {
                    QMutexLocker lock(TorcDevice::gDeviceListLock);

                    QHash<QString,TorcDevice*>::const_iterator it = TorcDevice::gDeviceList->constBegin();
                    for( ; it != TorcDevice::gDeviceList->constEnd(); ++it)
                        it.value()->Stop();
                }

                break;
            default: break;
        }
    }

    return QObject::event(Event);
}

/// Create the central controller object
class TorcCentralObject : public TorcAdminObject, public TorcStringFactory
{
    Q_DECLARE_TR_FUNCTIONS(TorcCentralObject)

  public:
    TorcCentralObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY - 5), // start last
        m_object(nullptr)
    {
        TorcCommandLine::RegisterEnvironmentVariable(QStringLiteral("TORC_NO_VALIDATION"), QStringLiteral("Disable validation of configuration file. This may speed up start times."));
    }

    ~TorcCentralObject()
    {
        Destroy();
    }

    void GetStrings(QVariantMap &Strings)
    {
        Strings.insert(QStringLiteral("CelsiusTr"),          QCoreApplication::translate("TorcCentral", "Celsius"));
        Strings.insert(QStringLiteral("CelsiusUnitsTr"),     QCoreApplication::translate("TorcCentral", "°C"));
        Strings.insert(QStringLiteral("FahrenheitTr"),       QCoreApplication::translate("TorcCentral", "Fahrenheit"));
        Strings.insert(QStringLiteral("FahrenheitUnitsTr"),  QCoreApplication::translate("TorcCentral", "°F"));
    }

    void Create(void)
    {
        Destroy();
        m_object = new TorcCentral();
    }

    void Destroy(void)
    {
        delete m_object;
        m_object = nullptr;
    }

  private:
    Q_DISABLE_COPY(TorcCentralObject)
    TorcCentral *m_object;
} TorcCentralObject;

TorcXSDFactory* TorcXSDFactory::gTorcXSDFactory = nullptr;

TorcXSDFactory::TorcXSDFactory()
  : nextTorcXSDFactory(gTorcXSDFactory)
{
    gTorcXSDFactory = this;
}

TorcXSDFactory* TorcXSDFactory::GetTorcXSDFactory(void)
{
    return gTorcXSDFactory;
}

TorcXSDFactory* TorcXSDFactory::NextFactory(void) const
{
    return nextTorcXSDFactory;
}

/*! \brief Customise the given base XSD with additional elements
*/
void TorcXSDFactory::CustomiseXSD(QByteArray &XSD)
{
    QStringList identifiers;
    identifiers << XSD_TYPES << XSD_INPUTTYPES << XSD_INPUTS << XSD_CONTROLTYPES << XSD_CONTROLS;
    identifiers << XSD_OUTPUTTYPES << XSD_OUTPUTS << XSD_NOTIFIERTYPES << XSD_NOTIFIERS;
    identifiers << XSD_NOTIFICATIONTYPES << XSD_NOTIFICATIONS << XSD_UNIQUE << XSD_CAMERATYPES;

    QMultiMap<QString,QString> xsds;
    TorcXSDFactory* factory = TorcXSDFactory::GetTorcXSDFactory();
    for ( ; factory; factory = factory->NextFactory())
        factory->GetXSD(xsds);

    foreach (const QString &ident, identifiers)
    {
        QString replacewith;
        QMultiMap<QString,QString>::const_iterator it = xsds.constBegin();
        for ( ; it != xsds.constEnd(); ++it)
            if (it.key() == ident)
                replacewith += it.value();
        XSD.replace(ident, replacewith.toLatin1());
    }
}
