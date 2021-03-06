/* Class TorcI2CPCA9685
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2015-18
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
#define PCA9685_RESOLUTION 4095
#define PCA9685_RANGE 4096

#include <unistd.h>
#include <math.h>

TorcI2CPCA9685Channel::TorcI2CPCA9685Channel(int Number, TorcI2CPCA9685 *Parent, const QVariantMap &Details)
  : TorcPWMOutput(0.0, PCA9685, Details, PCA9685_RANGE),
    m_channelNumber(Number),
    m_channelValue(0),
    m_parent(Parent)
{
    // PCA9685 only operates at default 12bit accuracy
    if (m_resolution != m_maxResolution)
    {
        m_resolution = m_maxResolution;
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Ignoring user defined resolution for PCA9685 channel - defaulting to %1").arg(m_maxResolution));
    }

    m_parent->SetPWM(m_channelNumber, m_channelValue);
}

TorcI2CPCA9685Channel::~TorcI2CPCA9685Channel()
{
    // NB we don't turn the channel off, we set it to its default (which is probably off)
    SetValue(defaultValue);
}

QStringList TorcI2CPCA9685Channel::GetDescription(void)
{
    return QStringList() << tr("I2C") << tr("PCA9685 16 Channel PWM") << QStringLiteral("%1 %2").arg(tr("Channel")).arg(m_channelNumber) << tr("Resolution %1").arg(m_resolution);
}

void TorcI2CPCA9685Channel::SetValue(double Value)
{
    QMutexLocker locker(&lock);

    // anything that doesn't change the range sent to the device is just noise, so
    // calculate now and filter early
    double newvalue = Value;
    if (!ValueIsDifferent(newvalue))
        return;

    // convert 0.0 to 1.0 to 0 to 4095
    int channelvalue = lround(newvalue * (float)PCA9685_RESOLUTION);
    if (channelvalue == m_channelValue)
        return;

    m_channelValue = channelvalue;
    m_parent->SetPWM(m_channelNumber, m_channelValue);
    TorcPWMOutput::SetValue(Value);
}
    
TorcI2CPCA9685::TorcI2CPCA9685(int Address, const QVariantMap &Details)
  : TorcI2CDevice(Address)
{
    // nullify outputs in case they aren't created
    memset(m_outputs, 0, sizeof(TorcI2CPCA9685Channel*) * 16);

    // open a handle to the device
    m_fd = wiringPiI2CSetup(m_address);
    if (m_fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to open I2C device at address 0x%1").arg(m_address, 0, 16));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Opened %1 I2C device at address 0x%2")
        .arg(PCA9685).arg(m_address, 0, 16));

    // reset
    if (wiringPiI2CWriteReg8(m_fd, MODE1, 0x00) < 0 || wiringPiI2CWriteReg8(m_fd, MODE2, 0x04) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to reset PCA9685 device"));
        return;
    }

    // set frequency to 1000Hz
    bool success = true;
    success &= wiringPiI2CWriteReg8(m_fd, MODE1, 0x01) > -1;
    success &= wiringPiI2CWriteReg8(m_fd, PRESCALE, (uint8_t)((CLOCKFREQ / PCA9685_RANGE / 1000) - 1)) > -1;
    success &= wiringPiI2CWriteReg8(m_fd, MODE1, 0x80) > -1;
    success &= wiringPiI2CWriteReg8(m_fd, MODE2, 0x04) > -1;

    if (!success)
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to set up PCA9685 device"));

    // create individual channel services
    // this will also reset each channel to the default value (0)
    QVariantMap::const_iterator it = Details.begin();
    for ( ; it != Details.constEnd(); ++it)
    {
        if (it.key() == QStringLiteral("channel"))
        {
            // channel needs a <number>
            QVariantMap channel = it.value().toMap();
            if (!channel.contains(QStringLiteral("number")))
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("PCA9685 channel has no number"));
                continue;
            }

            bool ok = false;
            int channelnum = channel.value(QStringLiteral("number")).toInt(&ok);

            if (!ok || channelnum < 0 || channelnum >= 16)
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to parse valid PCA9685 channel number from '%1'").arg(channel.value(QStringLiteral("number")).toString()));
                continue;
            }

            if (m_outputs[channelnum])
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("PCA9685 channel '%1' is already in use").arg(channelnum));
                continue;
            }

            m_outputs[channelnum] = new TorcI2CPCA9685Channel(channelnum, this, channel);
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
            m_outputs[i] = nullptr;
        }
    }

    // close device
    if (m_fd > -1)
        close(m_fd);
}

/*! \brief Set the hardware PWM for given channel.
 */
bool TorcI2CPCA9685::SetPWM(int Channel, int Value)
{
    if (m_fd < 0 || Channel < 0 || Channel > 15)
        return false;

    int offtime = qBound(0, Value, PCA9685_RESOLUTION);
    int ontime  = 0;

    // turn completely on or off if required
    // 'off' supercedes 'on' per spec
    if (offtime < 1)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Channel %1 turned completely OFF").arg(Channel));
        offtime |= 0x1000;
    }
    else if (offtime >= PCA9685_RESOLUTION)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Channel %1 turned completely ON").arg(Channel));
        ontime |= 0x1000;
    }

    bool success = true;
    success &= wiringPiI2CWriteReg8(m_fd, LED0_ON_LOW   + (4 * Channel), ontime & 0xFF) > -1;
    success &= wiringPiI2CWriteReg8(m_fd, LED0_ON_HIGH  + (4 * Channel), ontime >> 8) > -1;
    success &= wiringPiI2CWriteReg8(m_fd, LED0_OFF_LOW  + (4 * Channel), offtime & 0xFF) > -1;
    success &= wiringPiI2CWriteReg8(m_fd, LED0_OFF_HIGH + (4 * Channel), offtime >> 8) > -1;
    
    if (!success)
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error writing to channel %1").arg(Channel));
    return success;
}

class TorcI2CPCA9685Factory : public TorcI2CDeviceFactory
{
    TorcI2CDevice* Create(int Address, const QString &Name, const QVariantMap &Details)
    {
        if (PCA9685 == Name)
            return new TorcI2CPCA9685(Address, Details);
        return nullptr;
    }
} TorcI2CPCA9685Factory;
