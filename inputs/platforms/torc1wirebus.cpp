/* Class Torc1WireBus
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
#include <QDir>

// Torc
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcinputs.h"
#include "torc1wirebus.h"
#include "torc1wireds18b20.h"

Torc1WireBus* Torc1WireBus::gTorc1WireBus = new Torc1WireBus();

/*! \class Torc1WireBus
 *  \brief A class to handle the one wire files system (OWFS) for 1Wire devices.
 *
 * Using OWFS, 1Wire devices are presented at /sys/bus/w1/devices. If the kernel modules
 * have not been loaded the /sys/bus/w1 directory will not exist, although it may exist
 * and remain unpopulated if the wire module is still loaded.
 *
 * We no longer monitor the file system and create/remove devices as they are seen. Instead
 * we create those devices specified in the configuration file. Any that are not present
 * will remain invalid.
 *
 * \note Only tested with RaspberryPi under Raspbian; other devices/implementations may
 *       use a different filesystem structure.
 * \note Only the Maxim DS18B20 digital thermometer is currently supported.
 * \note The required kernel modules are not loaded by default on Raspbian. You will
 *       need to 'modprobe' for both w1_gpio and w1_therm and add these to /etc/modules
 *       to ensure they are loaded at each reboot.
 *
 * \code
 * <torc>
 *     <inputs>
 *         <wire1>
 *             <ds18b20>
 *                 <id>28-XXXXXXX</id>
 *                 <name>tanktemp</name>
 *                 <username>Tank temperature</username>
 *                 <userdescription>The temperature in the tank</userdescription>
 *             </ds18b20>
 *         </wire1>
 *     </inputs>
 * </torc>
 * \endcode
*/
Torc1WireBus::Torc1WireBus()
  : TorcDeviceHandler()
{
}

Torc1WireBus::~Torc1WireBus()
{
    Destroy();
}

void Torc1WireBus::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    // check for the correct directory
    QDir dir(ONE_WIRE_DIRECTORY);
    if (!dir.exists(ONE_WIRE_DIRECTORY))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find 1Wire directory (%1) - have you loaded the correct kernel modules").arg(ONE_WIRE_DIRECTORY));
        return;
    }

    QVariantMap::const_iterator i = Details.constBegin();
    for ( ; i != Details.constEnd(); i++)
    {
        // we look for 1Wire devices in <inputs>
        if (i.key() != SENSORS_DIRECTORY)
            continue;

        QVariantMap wire1 = i.value().toMap();
        QVariantMap::iterator it = wire1.begin();
        for ( ; it != wire1.end(); ++it)
        {
            // now we're looking for <wire1>
            // N.B. XML doesn't allow element names to begin with numeric characters - hence wire1

            if (it.key() == ONE_WIRE_NAME)
            {
                QVariantMap devices = it.value().toMap();
                QVariantMap::iterator it2 = devices.begin();
                for ( ; it2 != devices.end(); ++it2)
                {
                    QString devicetype  = it2.key();
                    QVariantMap details = it2.value().toMap();

                    // a 1Wire device must have the <id> field
                    if (!details.contains("id"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Cannot create 1Wire device without unique 1Wire ID ('%1' '%2')")
                            .arg(devicetype).arg(details.value("name").toString()));
                    }
                    else
                    {
                        TorcInput* device = NULL;
                        Torc1WireDeviceFactory* factory = Torc1WireDeviceFactory::GetTorc1WireDeviceFactory();
                        for ( ; factory; factory = factory->NextFactory())
                        {
                            device = factory->Create(devicetype, details);
                            if (device)
                                break;
                        }

                        if (device)
                        {
                            QString deviceid = details.value("id").toString();
                            LOG(VB_GENERAL, LOG_INFO, QString("New 1Wire device: %1").arg(deviceid));
                            m_inputs.insert(deviceid, device);
                        }
                        else
                        {
                            LOG(VB_GENERAL, LOG_WARNING, QString("Unable to find handler for 1Wire device type '%1'").arg(devicetype));
                        }
                    }

                }
            }
        }
    }
}

void Torc1WireBus::Destroy(void)
{
    QMutexLocker lock(m_lock);

    // delete any extant inputs
    QHash<QString,TorcInput*>::iterator it = m_inputs.begin();
    for ( ; it != m_inputs.end(); it++)
    {
        TorcInputs::gInputs->RemoveInput(it.value());
        it.value()->DownRef();
    }
    m_inputs.clear();
}

Torc1WireDeviceFactory* Torc1WireDeviceFactory::gTorc1WireDeviceFactory = NULL;

Torc1WireDeviceFactory::Torc1WireDeviceFactory()
{
    nextTorc1WireDeviceFactory = gTorc1WireDeviceFactory;
    gTorc1WireDeviceFactory = this;
}

Torc1WireDeviceFactory::~Torc1WireDeviceFactory()
{
}

Torc1WireDeviceFactory* Torc1WireDeviceFactory::GetTorc1WireDeviceFactory(void)
{
    return gTorc1WireDeviceFactory;
}

Torc1WireDeviceFactory* Torc1WireDeviceFactory::NextFactory(void) const
{
    return nextTorc1WireDeviceFactory;
}

static const QString wire1InputTypes =
"<xs:complexType name='ds18b20Type'>\r\n"
"  <xs:all>\r\n"
"    <xs:element name='name'            type='deviceNameType'/>\r\n"
"    <xs:element name='username'        type='userNameType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='userdescription' type='userDescriptionType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='wire1serial'     type='validStringType'/>\r\n"
"  </xs:all>\r\n"
"</xs:complexType>\r\n\r\n"
"<xs:complexType name='wire1Type'>\r\n"
"  <xs:choice minOccurs='1' maxOccurs='unbounded'>\r\n"
"    <xs:element name='ds18b20' type='ds18b20Type'/>\r\n"
"  </xs:choice>\r\n"
"</xs:complexType>\r\n";

static const QString wire1Inputs =
"    <xs:element minOccurs='0' maxOccurs='1' name='wire1'   type='wire1Type'/>\r\n";

static const QString wire1Unique =
"<!-- enforce unique 1Wire serial numbers -->\r\n"
"<xs:unique name='uniqueWire1Serial'>\r\n"
"<xs:selector xpath='.//wire1serial' />\r\n"
"  <xs:field xpath='.' />\r\n"
"</xs:unique>\r\n";

class Torc1WireXSDFactory : public TorcXSDFactory
{
  public:
    void GetXSD(QMultiMap<QString,QString> &XSD) {
        XSD.insert(XSD_INPUTTYPES, wire1InputTypes);
        XSD.insert(XSD_INPUTS, wire1Inputs);
    }

} Torc1WireXSDFactory;
