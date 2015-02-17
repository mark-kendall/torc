/* Class TorcSensors
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2014
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

// Torc
#include "torclogging.h"
#include "torcadminthread.h"
#include "torccentral.h"
#include "torcsensors.h"

#define BLACKLIST QString("")

TorcSensors* TorcSensors::gSensors = new TorcSensors();

/*! \brief TorcSensors contains the master record of known sensors.
 *
 * A static global TorcSensors object is created (this could alternatively be created
 * as a TorcAdminObject). This object will list these sensors as an HTTP service.
 *
 * Any subclass of TorcSensor will automatically register itself on creation. Any class
 * creating sensors must DownRef AND call RemoveSensor when deleting them.
 *
 * \todo Translations and constants for web client.
 * \note This class is thread safe.
*/
TorcSensors::TorcSensors()
  : QObject(),
    TorcHTTPService(this, SENSORS_DIRECTORY, "Sensors", TorcSensors::staticMetaObject, BLACKLIST),
    m_lock(new QMutex(QMutex::Recursive))
{
}

TorcSensors::~TorcSensors()
{
    delete m_lock;
}

void TorcSensors::Start(void)
{
    QMutexLocker locker(m_lock);

    foreach (TorcSensor *sensor, sensorList)
        sensor->Start();
}

void TorcSensors::Graph(void)
{
    QMutexLocker locker(m_lock);
    {
        QMutexLocker locker(TorcCentral::gStateGraphLock);
        TorcCentral::gStateGraph->append("subgraph cluster_0 {\r\nlabel = \"Sensors\";\r\nstyle=filled;\r\nfillcolor = \"grey95\";");

        foreach(TorcSensor *sensor, sensorList)
        {
            QString id = sensor->GetUserName().isEmpty() ? sensor->GetUniqueId() : sensor->GetUserName();
            TorcCentral::gStateGraph->append("    \"" + id + "\";\r\n");
        }

        TorcCentral::gStateGraph->append("}\r\n");
    }
}

void TorcSensors::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Return a map of known sensors.
 *
 * To simplify javascript on the webclient side, the map is presented as a list of sensor paths
 * by type, allowing a client to quickly filter and subscribe to the individual services for
 * further detail.
*/
QVariantMap TorcSensors::GetSensorList(void)
{
    QVariantMap result;
    QMutexLocker locker(m_lock);

    // iterate over our list for each sensor type
    for (int type = TorcSensor::Unknown; type < TorcSensor::MaxType; type++)
    {
        QStringList sensorsfortype;
        foreach (TorcSensor *sensor, sensorList)
            if (sensor->GetType() == type)
                sensorsfortype.append(TorcSensor::TypeToString(sensor->GetType()) + "/" + sensor->GetUniqueId());

        if (!sensorsfortype.isEmpty())
            result.insert(TorcSensor::TypeToString(static_cast<TorcSensor::Type>(type)), sensorsfortype);
    }
    return result;
}

void TorcSensors::AddSensor(TorcSensor *Sensor)
{
    QMutexLocker locker(m_lock);
    if (!Sensor)
        return;

    if (sensorList.contains(Sensor))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Already have a sensor named %1 - ignoring").arg(Sensor->GetUniqueId()));
        return;
    }

    Sensor->UpRef();
    sensorList.append(Sensor);
    LOG(VB_GENERAL, LOG_INFO, QString("Registered sensor '%1'").arg(Sensor->GetUniqueId()));
    emit SensorsChanged();
}

void TorcSensors::RemoveSensor(TorcSensor *Sensor)
{
    QMutexLocker locker(m_lock);
    if (!Sensor)
        return;

    if (!sensorList.contains(Sensor))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Sensor %1 not recognised - cannot remove").arg(Sensor->GetUniqueId()));
        return;
    }

    Sensor->DownRef();
    sensorList.removeOne(Sensor);
    LOG(VB_GENERAL, LOG_INFO, QString("Sensor %1 de-registered").arg(Sensor->GetUniqueId()));
    emit SensorsChanged();
}


void TorcSensors::UpdateSensor(const QString &Id, const QString &Name, const QString &Description)
{
    QMutexLocker locker(m_lock);

    foreach(TorcSensor* sensor, sensorList)
    {
        if (sensor->GetUniqueId() == Id)
        {
            if (!Name.isEmpty())
                sensor->SetUserName(Name);
            if (!Description.isEmpty())
                sensor->SetUserDescription(Description);
        }
    }
}
