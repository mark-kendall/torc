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
#include "../torcpwmoutput.h"
#include "torci2cpca9685.h"

class TorcI2CPCA9685Channel : public TorcPWMOutput
{
  public:
    TorcI2CPCA9685Channel(int Address, int Number, TorcI2CPCA9685 *Parent);
    ~TorcI2CPCA9685Channel();

  private:
    int             m_channelNumber;
    TorcI2CPCA9685 *m_parent;
};

TorcI2CPCA9685Channel::TorcI2CPCA9685Channel(int Address, int Number, TorcI2CPCA9685 *Parent)
  : TorcPWMOutput(0.0, "PCA9685", QString("I2C_PCA9685_0x%1_%2").arg(Address, 0, 16).arg(Number)),
    m_channelNumber(Number),
    m_parent(Parent)
{
}

TorcI2CPCA9685Channel::~TorcI2CPCA9685Channel()
{
}

TorcI2CPCA9685::TorcI2CPCA9685(int Address)
  : m_address(Address)
{
    // nullify outputs in case they aren't created
    memset(m_outputs, 0, 16);

    // TODO add all i2c gubbins here

    // create individual channel services
    for (int i = 0; i < 16; i++)
    {
        m_outputs[i] = new TorcI2CPCA9685Channel(m_address, i, this);
        TorcOutputs::gOutputs->AddOutput(m_outputs[i]);
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
}
