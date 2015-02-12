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

#ifdef USING_I2C
#include "outputs/platforms/torci2cbus.h"
#endif
#ifdef USING_PIGPIO
#include "outputs/platforms/torcpigpio.h"
#endif

#include "torccentral.h"

TorcCentral::TorcCentral()
  : QObject(),
    m_config(QVariantMap())
{
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
        LoadDevices();
        SensorsChanged();
        OutputsChanged();
    }
}

TorcCentral::~TorcCentral()
{
    // deregister for events
    gLocalContext->RemoveObserver(this);

#ifdef USING_PIGPIO
        TorcPiGPIO::gPiGPIO->CleanupPins();
#endif

#ifdef USING_I2C
    // cleanup any devices
    TorcI2CBus::gTorcI2CBus->CleanupDevices();
#endif
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

void TorcCentral::LoadDevices(void)
{
    // GPIO
    if (m_config.contains("GPIO"))
    {
        QVariantMap gpio = m_config.value("GPIO").toMap();

#ifdef USING_PIGPIO
        TorcPiGPIO::gPiGPIO->SetupPins(gpio);
#endif
    }

#ifdef USING_I2C
    // I2C
    // TODO move most of this code into TorcI2CBus
    if (m_config.contains("I2C"))
    {
        QVariantMap i2c = m_config.value("I2C").toMap();
        QVariantMap::iterator it = i2c.begin();
        for ( ; it != i2c.end(); ++it)
        {
            bool ok = false;
            // N.B. assumes hexadecimal 0xXX
            int address = it.key().toInt(&ok, 16);
            if (ok)
            {
                QVariantMap device = it.value().toMap();
                TorcI2CBus::gTorcI2CBus->SetupDevice(address, device);
            }
        }
    }
#endif
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
            TorcSensors::gSensors->UpdateSensor(it.key(), name, description);
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

