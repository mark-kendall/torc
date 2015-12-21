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
    TorcHTTPService(this, SENSORS_DIRECTORY, "sensors", TorcSensors::staticMetaObject, BLACKLIST),
    m_lock(new QMutex(QMutex::Recursive))
{
}

TorcSensors::~TorcSensors()
{
    delete m_lock;
}

QString TorcSensors::GetUIName(void)
{
    return tr("Sensors");
}

void TorcSensors::Start(void)
{
    QMutexLocker locker(m_lock);

    foreach (TorcSensor *sensor, sensorList)
        sensor->Start();
}

void TorcSensors::Reset(void)
{
    QMutexLocker locker(m_lock);

    foreach (TorcSensor *sensor, sensorList)
        sensor->Reset();
}


void TorcSensors::Graph(QByteArray *Data)
{
    if (!Data)
        return;

    Data->append("    subgraph cluster_0 {\r\n"
                 "        label = \"Sensors\";\r\n"
                 "        style=filled;\r\n"
                 "        fillcolor = \"grey95\";\r\n");

    foreach(TorcSensor *sensor, sensorList)
    {
        QString id    = sensor->GetUniqueId();
        QString label = sensor->GetUserName();
        QString desc;
        QStringList source = sensor->GetDescription();
        foreach (QString item, source)
            desc.append(QString(DEVICE_LINE_ITEM).arg(item));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Default %1").arg(sensor->GetDefaultValue())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Valid %1").arg(sensor->GetValid())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Value %1").arg(sensor->GetValue())));

        if (label.isEmpty())
            label = id;
        Data->append(QString("        \"%1\" [shape=record label=<<B>%2</B>%3> URL=\"%4Help\"];\r\n")
            .arg(id).arg(label).arg(desc).arg(sensor->Signature()));
    }

    Data->append("    }\r\n\r\n");
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
