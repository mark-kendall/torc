/* Class TorcPWMOutput
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
#include "torcpwmoutput.h"

TorcPWMOutput::TorcPWMOutput(double Value, const QString &ModelId, const QVariantMap &Details, uint MaxResolution)
  : TorcOutput(TorcOutput::PWM, Value, ModelId, Details),
    m_resolution(MaxResolution),
    m_maxResolution(MaxResolution)
{
    // check for user defined resolution
    // the XSD limits this to 7 to 24bit resolution
    QVariantMap::const_iterator it = Details.begin();
    for ( ; it != Details.constEnd(); ++it)
    {
        if (it.key() == "resolution")
        {
            bool ok = false;
            uint resolution = it.value().toUInt(&ok);
            if (ok)
            {
                if (resolution >= 128 && resolution <= 16777215)
                {
                    if (resolution > m_maxResolution)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Requested resolution of %1 for '%2' exceeds maximum - defaulting to %3")
                            .arg(resolution).arg(uniqueId).arg(m_maxResolution));
                    }
                    else
                    {
                        m_resolution = resolution;
                        LOG(VB_GENERAL, LOG_INFO, QString("Set resolution to %1 for '%2'").arg(m_resolution).arg(uniqueId));
                    }
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Requested resolution of %1 for '%2' is out of range - defaulting to %3")
                        .arg(resolution).arg(uniqueId).arg(m_maxResolution));
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse resolution for '%1'").arg(uniqueId));
            }
        }
    }
}

TorcPWMOutput::~TorcPWMOutput()
{
}

QStringList TorcPWMOutput::GetDescription(void)
{
    return QStringList() << tr("Constant PWM");
}

TorcOutput::Type TorcPWMOutput::GetType(void)
{
    return TorcOutput::PWM;
}

uint TorcPWMOutput::GetResolution(void)
{
    return m_resolution;
}

uint TorcPWMOutput::GetMaxResolution(void)
{
    return m_maxResolution;
}

bool TorcPWMOutput::ValueIsDifferent(double &NewValue)
{
    // range check
    double newvalue = qBound(0.0, NewValue, 1.0);

    // resolution check
    if (qAbs(value - newvalue) < (1.0 / (double)m_resolution))
        return false;

    // set new value in case range check caught out of bounds
    NewValue = newvalue;
    return true;
}
