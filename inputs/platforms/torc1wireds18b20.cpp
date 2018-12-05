/* Class Torc1WireDS18B20
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2014-18
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
#include <QtGlobal>
#include <QTextStream>

// Torc
#include "torclogging.h"
#include "torccentral.h"
#include "torc1wirebus.h"
#include "torc1wireds18b20.h"

#define MIN_READ_INTERVAL 10000

Torc1WireReader::Torc1WireReader(const QString &DeviceName)
  : m_timer(),
    m_file(QStringLiteral("%1%2/w1_slave").arg(ONE_WIRE_DIRECTORY, DeviceName))
{
    m_timer.setTimerType(Qt::CoarseTimer);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Torc1WireReader::Read);
    // randomise start time for each input
    m_timer.start(qrand() % MIN_READ_INTERVAL);
}

void Torc1WireReader::Read(void)
{
    if (!m_file.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to read device '%1' (Error #%2: '%3')")
                                 .arg(m_file.fileName()).arg(m_file.error()).arg(m_file.errorString()));
        emit NewTemperature(0.0, false);
        m_timer.start(MIN_READ_INTERVAL);
        return;
    }

    // read
    QTextStream text(&m_file);

    // check crc
    QString line = text.readLine();
    if (!line.contains(QStringLiteral("crc")) || !line.contains(QStringLiteral("YES")))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("CRC check failed for device %1").arg(m_file.fileName()));
        emit NewTemperature(0.0, false);
    }
    else
    {
        // read temp
        line = text.readLine().trimmed();
        int index = line.lastIndexOf(QStringLiteral("t="));
        if (index < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to parse temperature for device %1").arg(m_file.fileName()));
            emit NewTemperature(0.0, false);
        }
        else
        {
            line = line.mid(index + 2);
            bool ok = false;
            double temp = line.toDouble(&ok);
            if (ok)
            {
                emit NewTemperature(temp / 1000.0, true);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to convert temperature for device %1").arg(m_file.fileName()));
                emit NewTemperature(0.0, false);
            }
        }
    }
    m_file.close();
    m_timer.start(MIN_READ_INTERVAL);
}

Torc1WireReadThread::Torc1WireReadThread(Torc1WireDS18B20 *Parent, const QString &DeviceName)
  : TorcQThread(QStringLiteral("1Wire")),
    m_parent(Parent),
    m_reader(nullptr),
    m_deviceName(DeviceName)
{
}

void Torc1WireReadThread::Start(void)
{
    if (m_parent)
    {
        m_reader = new Torc1WireReader(m_deviceName);
        connect(m_reader, &Torc1WireReader::NewTemperature, m_parent, &Torc1WireDS18B20::Read);
    }
}

void Torc1WireReadThread::Finish(void)
{
    if (m_reader)
        delete m_reader;
    m_reader = nullptr;
    if (m_parent)
        m_parent->Read(0.0, false);
}

Torc1WireDS18B20::Torc1WireDS18B20(const QVariantMap &Details)
  : TorcTemperatureInput(TorcCentral::GetGlobalTemperatureUnits() == TorcCentral::Celsius ? 0.0 : 32.0,
                         TorcCentral::GetGlobalTemperatureUnits() == TorcCentral::Celsius ? -55.0 : -67.0,
                         TorcCentral::GetGlobalTemperatureUnits() == TorcCentral::Celsius ? 125.0 : 257.0,
                         DS18B20NAME, Details),
    m_deviceId(Details.value(QStringLiteral("wire1serial")).toString()),
    m_readThread(this, Details.value(QStringLiteral("wire1serial")).toString())
{
    m_readThread.start();
}

Torc1WireDS18B20::~Torc1WireDS18B20()
{
    m_readThread.quit();
    m_readThread.wait();
}

QStringList Torc1WireDS18B20::GetDescription(void)
{
    return QStringList() << tr("1Wire DS18B20 Temperature" ) << tr("Serial# %1").arg(m_deviceId);
}

void Torc1WireDS18B20::Read(double Value, bool Valid)
{
    if (Valid)
    {
        // readings are in celsius - convert if needed
        double value = Value;
        if (TorcCentral::GetGlobalTemperatureUnits() == TorcCentral::Fahrenheit)
            value = TorcTemperatureInput::CelsiusToFahrenheit(Value);
        SetValue(value);
    }
    else
    {
        SetValid(false);
    }
}

class Torc1WireDS18B20Factory : public Torc1WireDeviceFactory
{
    TorcInput* Create(const QString &DeviceType, const QVariantMap &Details)
    {
        if (DeviceType == DS18B20NAME)
            return new Torc1WireDS18B20(Details);
        return nullptr;
    }
} Torc1WireDS18B20Factory;

          
