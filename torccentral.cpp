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

// for system
#include <stdlib.h>

QByteArray* TorcCentral::gStateGraph = new QByteArray();
QMutex*     TorcCentral::gStateGraphLock = new QMutex(QMutex::Recursive);

TorcCentral::TorcCentral()
  : QObject(),
    m_config(QVariantMap())
{
    // reset state graph
    // content directory should already have been created by TorcHTMLDynamicContent
    QString graphdot = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "stategraph.dot";
    QString graphsvg = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "stategraph.svg";
    QString config   = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "torc.xml";
    QString current  = GetTorcConfigDir() + "/torc.xml";

    {
        QMutexLocker locker(gStateGraphLock);
        gStateGraph->clear();
        gStateGraph->append("strict digraph G {\r\n"
                            "    rankdir=\"LR\";\r\n"
                            "    node [shape=rect];\r\n");

        if (QFile::exists(graphdot))
            QFile::remove(graphdot);
        if (QFile::exists(graphsvg))
            QFile::remove(graphsvg);
        if (QFile::exists(config))
            QFile::remove(config);

        QFile currentconfig(current);
        if (!currentconfig.copy(config))
            LOG(VB_GENERAL, LOG_WARNING, "Failed to copy current config file to content directory");
    }

    // listen for interesting events
    gLocalContext->AddObserver(this);

    if (LoadConfig())
    {
        TorcDeviceHandler::Start(m_config);

        // start building the graph
        TorcSensors::gSensors->Graph();
        TorcOutputs::gOutputs->Graph();
        TorcControls::gControls->Graph();

        // connect controls to sensors/outputs/other controls
        TorcControls::gControls->Validate();

        // complete the state graph
        {
            QMutexLocker locker(gStateGraphLock);
            gStateGraph->append(QString("    label=\"%1\";\r\n"
                                        "    labelloc=\"t\";\r\n"
                                        "    URL=\"/\";\r\n"
                                        "}\r\n").arg(QCoreApplication::applicationName()));

            QFile file(graphdot);
            if (file.open(QIODevice::ReadWrite))
            {
                file.write(*gStateGraph);
                file.flush();
                file.close();
                LOG(VB_GENERAL, LOG_INFO, QString("Saved state graph as %1").arg(graphsvg));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' to write state graph").arg(graphdot));
            }
        }

        // create a representation of the state graph
        // NB QProcess appears to be fatally broken. Just use system instead
        QString command = QString("dot -Tsvg -o %1 %2").arg(graphsvg).arg(graphdot);
        int err = system(command.toLocal8Bit());
        if (err < 0)
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed to create stategraph representation (err: %1)").arg(strerror(err)));
        else
            LOG(VB_GENERAL, LOG_INFO, QString("Saved state graph representation as %1").arg(graphsvg));

        // initialise the state machine
        TorcSensors::gSensors->Start();
        TorcControls::gControls->Start();
    }
}

TorcCentral::~TorcCentral()
{
    // deregister for events
    gLocalContext->RemoveObserver(this);

    // cleanup any devices
    TorcDeviceHandler::Stop();
}

bool TorcCentral::LoadConfig(void)
{
    // load the config file
    QString xml = GetTorcConfigDir() + "/torc.xml";
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
    if (m_config.contains("name"))
    {
        QString name = m_config.value("name").toString();
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
