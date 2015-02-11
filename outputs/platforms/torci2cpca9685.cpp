/* Class TorcI2CPCA9685
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "../torcpwmoutput.h"
#include "torci2cpca9685.h"

// wiringPi
#include <wiringPiI2C.h>

// PCA9685
#include <stdint.h>
#define MODE1         0x00
#define MODE2         0x01
#define LED0_ON_LOW   0x06
#define LED0_ON_HIGH  0x07
#define LED0_OFF_LOW  0x08
#define LED0_OFF_HIGH 0x09
#define PRESCALE      0xFE
#define CLOCKFREQ     25000000.0

class TorcI2CPCA9685Channel : public TorcPWMOutput
{
  public:
    TorcI2CPCA9685Channel(int Address, int Number, TorcI2CPCA9685 *Parent);
    ~TorcI2CPCA9685Channel();

    void SetValue (double Value);

  private:
    int             m_channelNumber;
    TorcI2CPCA9685 *m_parent;
};

TorcI2CPCA9685Channel::TorcI2CPCA9685Channel(int Address, int Number, TorcI2CPCA9685 *Parent)
  : TorcPWMOutput(0.0, PCA9685, QString("I2C_PCA9685_0x%1_%2").arg(Address, 0, 16).arg(Number)),
    m_channelNumber(Number),
    m_parent(Parent)
{
    m_parent->SetPWM(m_channelNumber, value);
}

TorcI2CPCA9685Channel::~TorcI2CPCA9685Channel()
{
    // always turn the output off completely on exit
    m_parent->SetPWM(m_channelNumber, 0.0);
}

void TorcI2CPCA9685Channel::SetValue(double Value)
{
    QMutexLocker locker(m_lock);

    // ignore same value updates
    if (qFuzzyCompare(Value + 1.0f, value + 1.0f))
        return;

    m_parent->SetPWM(m_channelNumber, Value);
    TorcPWMOutput::SetValue(Value);
}
    
TorcI2CPCA9685::TorcI2CPCA9685(int Address, const QVariantMap &Details)
  : TorcI2CDevice(Address)
{
    // nullify outputs in case they aren't created
    memset(m_outputs, 0, 16);

    // open a handle to the device
    m_fd = wiringPiI2CSetup(m_address);
    if (m_fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open I2C device at address 0x%1").arg(m_address, 0, 16));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Opened %1 I2C device at address 0x%2")
        .arg(PCA9685).arg(m_address, 0, 16));

    // reset
    if (wiringPiI2CWriteReg8(m_fd, MODE1, 0x00) < 0 || wiringPiI2CWriteReg8(m_fd, MODE2, 0x04) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to reset PCA9685 device");
        return;
    }

    // set frequency to 1000Hz
    // stop error checking here:)
    (int)wiringPiI2CWriteReg8(m_fd, MODE1, 0x01);
    (int)wiringPiI2CWriteReg8(m_fd, PRESCALE, (uint8_t)((CLOCKFREQ / 4096 / 1000) -1));
    (int)wiringPiI2CWriteReg8(m_fd, MODE1, 0x80);
    (int)wiringPiI2CWriteReg8(m_fd, MODE2, 0x04);

    // create individual channel services
    // this will also reset each channel to the default value (0)
    QVariantMap channels = Details.value("channels").toMap();

    for (int i = 0; i < 16; i++)
    {
        m_outputs[i] = new TorcI2CPCA9685Channel(m_address, i, this);
        TorcOutputs::gOutputs->AddOutput(m_outputs[i]);

        if (channels.contains(QString::number(i)))
        {
            QVariantMap details = channels.value(QString::number(i)).toMap();
            if (details.contains("userName"))
                m_outputs[i]->SetUserName(details.value("userName").toString());
            if (details.contains("userDescription"))
                m_outputs[i]->SetUserDescription(details.value("userDescription").toString());
        }
    }
}

TorcI2CPCA9685::~TorcI2CPCA9685()
{
    // remove channels
    for (int i = 0; i < 16; i++)
    {
        if (m_outputs[i])
        {
            TorcOutputs::gOutputs->RemoveOutput(m_outputs[i]);
            m_outputs[i]->DownRef();
            m_outputs[i] = NULL;
        }
    }

    // close device
    if (m_fd > -1)
        close(m_fd);
}

/*! \brief Set the hardware PWM for given channel.
 *
 * \todo Randomise on time?
 * \todo check return values
 */
bool TorcI2CPCA9685::SetPWM(int Channel, double Value)
{
    if (m_fd < 0 || Channel < 0 || Channel > 15)
        return false;

    // sanity check range
    double value = Value;
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;

    // convert 0.0 to 1.0 to 0 to 4095
    int offtime = (int)((value * 4095.0) + 0.5);
    int ontime  = 0;

    // turn completely on or off if required
    // 'off' supercedes 'on' per spec
    if (offtime < 1)
        offtime |= 0x1000;
    else if (offtime > 4094)
        ontime |= 0x1000;

    (int)wiringPiI2CWriteReg8(m_fd, LED0_ON_LOW   + (4 * Channel), ontime & 0xFF);
    (int)wiringPiI2CWriteReg8(m_fd, LED0_ON_HIGH  + (4 * Channel), ontime >> 8);
    (int)wiringPiI2CWriteReg8(m_fd, LED0_OFF_LOW  + (4 * Channel), offtime & 0xFF);
    (int)wiringPiI2CWriteReg8(m_fd, LED0_OFF_HIGH + (4 * Channel), offtime >> 8);
    
    return true;
}

class TorcI2CPCA9685Factory : public TorcI2CDeviceFactory
{
    TorcI2CDevice* Create(int Address, const QString &Name, const QVariantMap &Details)
    {
        LOG(VB_GENERAL, LOG_INFO, "XXXXX");
        if (PCA9685 == Name)
            return new TorcI2CPCA9685(Address, Details);
        return NULL;
    }
} TorcI2CPCA9685Factory;
