/* Class TorcCentral
*
* Copyright (C) Mark Kendall 2015
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
#include <QProcess>
#include <QJsonDocument>
#include <QCoreApplication>

// Torc
#include "torcevent.h"
#include "torclogging.h"
#include "torcdirectories.h"
#include "torclocalcontext.h"
#include "torc/torcadminthread.h"
#include "sensors/torcsensors.h"
#include "outputs/torcoutputs.h"
#include "control/torccontrols.h"
#include "http/torchttphandler.h"
#include "torcxmlreader.h"
#include "torccentral.h"

#ifdef USING_XMLPATTERNS
#include "torcxmlvalidator.h"
#endif

// for system
#include <stdlib.h>

TorcCentral::TorcCentral()
  : QObject(),
    m_config(QVariantMap()),
    m_graph(new QByteArray())
{
    // reset state graph and clear out old files
    // content directory should already have been created by TorcHTMLDynamicContent
    QString graphdot = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "stategraph.dot";
    QString graphsvg = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "stategraph.svg";
    QString config   = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "torc.xml";
    QString current  = GetTorcConfigDir() + "/torc.xml";

    if (QFile::exists(graphdot))
        QFile::remove(graphdot);
    if (QFile::exists(graphsvg))
        QFile::remove(graphsvg);
    if (QFile::exists(config))
        QFile::remove(config);

    QFile currentconfig(current);
    if (!currentconfig.copy(config))
        LOG(VB_GENERAL, LOG_WARNING, "Failed to copy current config file to content directory");

    // listen for interesting events
    gLocalContext->AddObserver(this);

    if (LoadConfig())
    {
        // create the devices
        TorcDeviceHandler::Start(m_config);

        // connect controls to sensors/outputs/other controls
        TorcControls::gControls->Validate();

        // initialise the state machine
        TorcSensors::gSensors->Start();
        TorcControls::gControls->Start();

        // iff we have got this far, then create the graph
        // start the graph
        m_graph->append(QString("strict digraph \"%1\" {\r\n"
                                    "    rankdir=\"LR\";\r\n"
                                    "    node [shape=rect];\r\n")
                            .arg(QCoreApplication::applicationName()));

        // build the graph contents
        TorcSensors::gSensors->Graph(m_graph);
        TorcOutputs::gOutputs->Graph(m_graph);
        TorcControls::gControls->Graph(m_graph);

        // complete the graph
        m_graph->append(QString("    label=\"%1\";\r\n"
                                    "    labelloc=\"t\";\r\n"
                                    "    URL=\"/\";\r\n"
                                    "}\r\n").arg(QCoreApplication::applicationName()));

        QFile file(graphdot);
        if (file.open(QIODevice::ReadWrite))
        {
            file.write(*m_graph);
            file.flush();
            file.close();
            LOG(VB_GENERAL, LOG_INFO, QString("Saved state graph as %1").arg(graphsvg));

            // create a representation of the state graph
            // NB QProcess appears to be fatally broken. Just use system instead
            QString command = QString("dot -Tsvg -o %1 %2").arg(graphsvg).arg(graphdot);
            int err = system(command.toLocal8Bit());
            if (err < 0)
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to create stategraph representation (err: %1)").arg(strerror(err)));
            else
                LOG(VB_GENERAL, LOG_INFO, QString("Saved state graph representation as %1").arg(graphsvg));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' to write state graph").arg(graphdot));
        }
    }

    // no need for the graph data from here
    m_graph->clear();
}

TorcCentral::~TorcCentral()
{
    // deregister for events
    gLocalContext->RemoveObserver(this);

    // cleanup any devices
    TorcDeviceHandler::Stop();

    // cleanup graph
    delete m_graph;
}

bool TorcCentral::LoadConfig(void)
{
    // load the config file
    QString xml = GetTorcConfigDir() + "/torc.xml";

#ifdef USING_XMLPATTERNS
    if (!qgetenv("TORC_NO_VALIDATION").isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, "Skipping configuration file validation.");
    }
    else
    {
        QString xsd = GetTorcShareDir() + "/html/torc.xsd";

        LOG(VB_GENERAL, LOG_INFO, "Starting validation of configuration file");

        TorcXmlValidator validator(xml, xsd);
        if (!validator.Validated())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Configuration file '%1' failed validation").arg(xml));
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, "Configuration successfully validated");
    }
#else
    LOG(VB_GENERAL, LOG_INFO, "QXmlPatterns unavailable - not validating configuration file.");
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
    if (!result.contains("torc"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find 'torc' root element in '%1'").arg(xml));
        return false;
    }

    m_config = result.value("torc").toMap();

    // update application name
    if (m_config.contains("applicationname"))
    {
        QString name = m_config.value("applicationname").toString();
        if (!name.isEmpty())
        {
            QCoreApplication::setApplicationName(name);
            LOG(VB_GENERAL, LOG_INFO, QString("Changed application name to '%1'").arg(name));
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Loaded config from %1").arg(xml));
    return true;
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
            case Torc::Exit:
                TorcReferenceCounter::EventLoopEnding(true);
                QCoreApplication::quit();
                break;
            case Torc::ShuttingDown:
            case Torc::Suspending:
            case Torc::Restarting:
            case Torc::Hibernating:
                // NB this just ensures outputs (e.g. PWM drivers) are set to sensible
                // defaults if we shut down. There is currently no handling of waking, which
                // would need more thought about resetting state etc.
                LOG(VB_GENERAL, LOG_INFO, "Resetting devices to defaults.");
                TorcSensors::gSensors->Reset();
                TorcControls::gControls->Reset();
                TorcOutputs::gOutputs->Reset();
                break;
            default: break;
        }
    }

    return QObject::event(Event);
}

/// Create the central controller object
class TorcCentralObject : public TorcAdminObject
{
  public:
    TorcCentralObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY - 5), // start last
        m_object(NULL)
    {
        TorcCommandLine::RegisterEnvironmentVariable("TORC_NO_VALIDATION", "Disable validation of configuration file. This may speed up start times.");
    }

    ~TorcCentralObject()
    {
    }

    void Create(void)
    {
        Destroy();
        m_object = new TorcCentral();
    }

    void Destroy(void)
    {
        delete m_object;
        m_object = NULL;
    }

  private:
    TorcCentral *m_object;
} TorcCentralObject;
