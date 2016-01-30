/* Class Torc1WireDS18B20
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2014-16
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
#include <QFile>
#include <QtGlobal>
#include <QTextStream>

// Torc
#include "torclogging.h"
#include "torc1wirebus.h"
#include "torc1wireds18b20.h"

#define MIN_READ_INTERVAL 10000

Torc1WireReadThread::Torc1WireReadThread(Torc1WireDS18B20 *Parent, const QString &Filename)
  : TorcQThread("1Wire"),
    m_parent(Parent),
    m_timer(NULL),
    m_file(ONE_WIRE_DIRECTORY + Filename + "/w1_slave") 
{   
}   

void Torc1WireReadThread::Start(void)
{
    // create and setup timer in correct thread
    m_timer = new QTimer();
    m_timer->setTimerType(Qt::CoarseTimer);

    // restart the timer each time to handle periods of load/delay
    m_timer->setSingleShot(true);

    // 'this' is in the parent's thread and connect will by default use an indirect connection,
    // so force a direct connection to ensure Read operates in the 1Wire thread.
    connect(m_timer, SIGNAL(timeout()), this, SLOT(Read()), Qt::DirectConnection);

    // randomise start time for each input
    m_timer->start(qrand() % MIN_READ_INTERVAL);
}

void Torc1WireReadThread::Finish(void)
{
    delete m_timer;
    m_timer = NULL;
}

void Torc1WireReadThread::Read(void)
{
    QFile file(m_file);

    // open
    if (!file.open(QIODevice::ReadOnly))
    {   
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to read device '%1' (Error #%2: '%3')")
                                 .arg(m_file).arg(file.error()).arg(file.errorString()));
    }
    else
    {   
        // read
        QTextStream text(&file);
        
        // check crc 
        QString line = text.readLine();
        if (!line.contains("crc") || !line.contains("YES"))
        {   
            LOG(VB_GENERAL, LOG_ERR, QString("CRC check failed for device %1").arg(m_file));
        }
        else
        {   
            // read temp
            line = text.readLine().trimmed();
            int index = line.lastIndexOf("t=");
            if (index < 0)
            {   
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse temperature for device %1").arg(m_file));
            }
            else
            {   
                line = line.mid(index + 2);
                bool ok = false;
                double temp = line.toDouble(&ok);
                if (ok)
                {
                    file.close();
                    m_parent->Read(temp / 1000.0, true);
                    m_timer->start(MIN_READ_INTERVAL);
                    return;
                }   
                
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to convert temperature for device %1").arg(m_file));
            }   
        }   
    }   
    
    file.close();
    m_parent->Read(0, false);

    m_timer->start(MIN_READ_INTERVAL);
}

Torc1WireDS18B20::Torc1WireDS18B20(const QVariantMap &Details)
  : TorcTemperatureInput(TorcTemperatureInput::Celsius, 0, -55.0, 125.0, DS18B20NAME, Details),
    m_deviceId(""),
    m_readThread(NULL)
{
    m_deviceId = Details.value("id").toString();
    m_readThread = new Torc1WireReadThread(this, m_deviceId);
    m_readThread->start();
}

Torc1WireDS18B20::~Torc1WireDS18B20()
{
    m_readThread->quit();
    m_readThread->wait();
    delete m_readThread;
    m_readThread = NULL;
}

QStringList Torc1WireDS18B20::GetDescription(void)
{
    return QStringList() << tr("1Wire DS18B20 Temperature" ) << tr("Serial# %1").arg(m_deviceId);
}

void Torc1WireDS18B20::Read(double Value, bool Valid)
{
    if (Valid)
        SetValue(Value);
    else
        SetValid(false);
}

class Torc1WireDS18B20Factory : public Torc1WireDeviceFactory
{
    TorcInput* Create(const QString &DeviceType, const QVariantMap &Details)
    {
        if (DeviceType == DS18B20NAME)
            return new Torc1WireDS18B20(Details);
        return NULL;
    }
} Torc1WireDS18B20Factory;

          
