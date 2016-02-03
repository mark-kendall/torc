/* Class TorcPiSwitchInput
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2015-16
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
#include "torcpigpio.h"
#include "torcpiswitchinput.h"

// wiringPi
#include "wiringPi.h"

#include <poll.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DEFAULT_VALUE 0

TorcPiSwitchInputThread::TorcPiSwitchInputThread(TorcPiSwitchInput *Parent, int Pin)
  : TorcQThread(QString("GPIOInput%1").arg(Pin)),
    m_parent(Parent),
    m_pin(Pin),
    m_aborted(false),
    m_file(QString("/sys/class/gpio/gpio%1/value").arg(wpiPinToGpio(Pin)))
{
   connect(this, SIGNAL(Changed(double)), m_parent, SLOT(SetValue(double)));
}

TorcPiSwitchInputThread::~TorcPiSwitchInputThread()
{
}

void TorcPiSwitchInputThread::Finish(void)
{
    // close input file
    m_file.close();

    // unexport the pin. There shouldn't be anything else using it
    QFile unexport("/sys/class/gpio/unexport");
    if (unexport.open(QIODevice::WriteOnly))
    {
        QByteArray pin = QByteArray::number(wpiPinToGpio(m_pin));
        pin.append("\n");
        if (unexport.write(pin) > -1)
            LOG(VB_GENERAL, LOG_INFO, QString("Unexported pin %1").arg(m_pin));

        unexport.close();
    }
}

void TorcPiSwitchInputThread::Update(void)
{
    int value = digitalRead(m_pin);
    emit Changed((double)value);
    LOG(VB_GENERAL, LOG_INFO, QString::number(value));
}

void TorcPiSwitchInputThread::Start(void)
{
    // disable any internall pull up/down resistors
    pullUpDnControl(m_pin, PUD_OFF);

    int bcmpin = wpiPinToGpio(m_pin);

    QFile export1("/sys/class/gpio/export");
    if (!export1.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' for writing").arg(export1.fileName()));
        return;
    }

    QByteArray pin = QByteArray::number(bcmpin);
    pin.append("\n");
    if (export1.write(pin) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to write to '%1'").arg(export1.fileName()));
        export1.close();
        return;
    }
    export1.close();
    LOG(VB_GENERAL, LOG_INFO, QString("Exported pin %1").arg(m_pin));

    QFile direction(QString("/sys/class/gpio/gpio%1/direction").arg(bcmpin));
    if (!direction.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' for writing").arg(direction.fileName()));
        return;
    }

    QByteArray dir("in\n");
    if (direction.write(dir) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to write to '%1'").arg(direction.fileName()));
        direction.close();
        return;
    }
    direction.close();
    LOG(VB_GENERAL, LOG_INFO, QString("Pin %1 set as input").arg(m_pin));

    QFile edge(QString("/sys/class/gpio/gpio%1/edge").arg(bcmpin));
    if (!edge.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' for writing").arg(edge.fileName()));
        return;
    }

    QByteArray both("both\n");
    if (edge.write(both) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to write to '%1'").arg(edge.fileName()));
        edge.close();
        return;
    }
    edge.close();

    // and finally, open the pin value for monitoring
    if (!m_file.open(QIODevice::ReadWrite))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' to monitor GPIO input (err: %2)")
            .arg(m_file.fileName()).arg(strerror(errno)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Pin %1 input setup complete").arg(m_pin));
    }
}

void TorcPiSwitchInputThread::run(void)
{
    Initialise();

    if (m_file.isOpen())
    {
        int count;
        quint8 c;

        // clear any pending interrupts    
        ioctl(m_file.handle(), FIONREAD, &count);
        for (int i = 0; i < count; i++)
            read(m_file.handle(), &c, 1);
        lseek(m_file.handle(), 0, SEEK_SET);

        // read the initial state
        Update();

        while (!m_aborted)
        {
            struct pollfd polls;
            polls.fd = m_file.handle();
            polls.events = POLLPRI;

            // 10ms timeout - reasonable?
            int res = poll(&polls, 1, 10);
        
            if (res == 0)
            {
                continue; // timeout
            }
            else if (res < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Poll failed for '%1' (err: %2)")
                    .arg(m_file.fileName()).arg(strerror(errno)));
                continue;
            }
            else
            {
                // clear the interrupt
                (void)read(m_file.handle(), &c, 1);
                lseek(m_file.handle(), 0, SEEK_SET);

                // read the value
                Update();

                // crude debounce handling
                // wait 20ms before polling again
                msleep(20);
            }    
        }
    }

    Deinitialise();
}

void TorcPiSwitchInputThread::Stop(void)
{
    m_aborted = true;
}

TorcPiSwitchInput::TorcPiSwitchInput(int Pin, const QVariantMap &Details)
  : TorcSwitchInput(DEFAULT_VALUE, "PiGPIOSwitchInput", Details),
    m_pin(Pin),
    m_inputThread(new TorcPiSwitchInputThread(this, m_pin))
{
}

TorcPiSwitchInput::~TorcPiSwitchInput()
{
    m_inputThread->Stop();
    m_inputThread->quit();
    m_inputThread->wait();
    delete m_inputThread;
}

void TorcPiSwitchInput::Start(void)
{
    // start listening for interrupts here...
    m_inputThread->start();

    // and call the default implementation
    TorcSwitchInput::Start();
}

QStringList TorcPiSwitchInput::GetDescription(void)
{
    return QStringList() << tr("Pin %1 Switch").arg(m_pin);
}


