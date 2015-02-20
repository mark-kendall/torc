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
    QString config   = GetTorcConfigDir() + DYNAMIC_DIRECTORY + "torc.config";
    QString current  = GetTorcConfigDir() + "/torc.config";

    {
        QMutexLocker locker(gStateGraphLock);
        gStateGraph->clear();
        gStateGraph->append("strict digraph G {\r\n    rankdir=\"LR\";\r\n    node [shape=rect];\r\n");

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

    // listen for sensor and output updates
    // currently will only be notified of 1Wire hotplugged temperature sensors
    if (TorcSensors::gSensors)
        connect(TorcSensors::gSensors, SIGNAL(SensorsChanged()), this, SLOT(SensorsChanged()));

    if (TorcOutputs::gOutputs)
        connect(TorcOutputs::gOutputs, SIGNAL(OutputsChanged()), this, SLOT(OutputsChanged()));

    if (LoadConfig())
    {
        TorcDeviceHandler::Start(m_config);
        SensorsChanged();
        OutputsChanged();

        // start building the graph
        TorcSensors::gSensors->Graph();
        TorcOutputs::gOutputs->Graph();
        TorcControls::gControls->Graph();

        // connect controls to sensors/outputs/other controls
        TorcControls::gControls->Validate();

        // complete the state graph
        {
            QMutexLocker locker(gStateGraphLock);
            gStateGraph->append(QString("\r\n    label=\"%1\";\r\nlabelloc=\"t\";\r\nURL=\"/\";\r\n}\r\n").arg(QCoreApplication::applicationName()));

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
    QString config = GetTorcConfigDir() + "/torc.config";

    if (!QFile::exists(config))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 does not exist").arg(config));
        return false;
    }

    QFile file(config);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot open %1 for reading").arg(config));
        return false;
    }

    QByteArray   data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    file.close();

    if (doc.isNull())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 is not valid JSON").arg(config));
        return false;
    }

    QVariant contents = doc.toVariant();

    // check the type
    if (QMetaType::QVariantMap != static_cast<QMetaType::Type>(contents.type()))
    {
        LOG(VB_GENERAL, LOG_ERR, "Config has unexpected JSON type");
        return false;
    }

    m_config = contents.toMap();

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

    LOG(VB_GENERAL, LOG_INFO, QString("Loaded config from %1").arg(config));
    return true;
}

void TorcCentral::SensorsChanged(void)
{
    // any 1Wire devices identified
    if (m_config.contains("1Wire"))
    {
        QVariantMap wire = m_config.value("1Wire").toMap();
        QVariantMap::Iterator it = wire.begin();
        for ( ; it != wire.end(); ++it)
        {
            QVariantMap device = it.value().toMap();
            QString name = device.value("userName").toString();
            QString description = device.value("userDescription").toString();
            TorcSensors::gSensors->UpdateSensor("1Wire_" + it.key(), name, description);
        }
    }
}

void TorcCentral::OutputsChanged(void)
{
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

QList<TorcDeviceHandler*> TorcDeviceHandler::gTorcDeviceHandlers;
TorcDeviceHandler* TorcDeviceHandler::gTorcDeviceHandler = NULL;
QMutex* TorcDeviceHandler::gTorcDeviceHandlersLock = new QMutex(QMutex::Recursive);

TorcDeviceHandler::TorcDeviceHandler()
  : m_lock(new QMutex(QMutex::Recursive))
{
    QMutexLocker lock(gTorcDeviceHandlersLock);
    m_nextTorcDeviceHandler = gTorcDeviceHandler;
    gTorcDeviceHandler = this;
}

TorcDeviceHandler::~TorcDeviceHandler()
{
    delete m_lock;
    m_lock = NULL;
}

TorcDeviceHandler* TorcDeviceHandler::GetNextHandler(void)
{
    return m_nextTorcDeviceHandler;
}

TorcDeviceHandler* TorcDeviceHandler::GetDeviceHandler(void)
{
    return gTorcDeviceHandler;
}

void TorcDeviceHandler::Start(const QVariantMap &Details)
{
    TorcDeviceHandler* handler = TorcDeviceHandler::GetDeviceHandler();
    for ( ; handler; handler = handler->GetNextHandler())
        handler->Create(Details);
}

void TorcDeviceHandler::Stop(void)
{
    TorcDeviceHandler* handler = TorcDeviceHandler::GetDeviceHandler();
    for ( ; handler; handler = handler->GetNextHandler())
        handler->Destroy();
}


