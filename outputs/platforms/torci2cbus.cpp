/* Class TorcI2CBus
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torclogging.h"
#include "torccentral.h"
#include "torcinput.h"
#include "torcoutput.h"
#include "torci2cbus.h"

TorcI2CDevice::TorcI2CDevice(int Address)
  : m_address(Address),
    m_fd(-1)
{
}

TorcI2CDevice::~TorcI2CDevice()
{
}

TorcI2CDeviceFactory* TorcI2CDeviceFactory::gTorcI2CDeviceFactory = NULL;

TorcI2CDeviceFactory::TorcI2CDeviceFactory()
{
    nextTorcI2CDeviceFactory = gTorcI2CDeviceFactory;
    gTorcI2CDeviceFactory = this;
}

TorcI2CDeviceFactory::~TorcI2CDeviceFactory()
{
}

TorcI2CDeviceFactory* TorcI2CDeviceFactory::GetTorcI2CDeviceFactory(void)
{
    return gTorcI2CDeviceFactory;
}

TorcI2CDeviceFactory* TorcI2CDeviceFactory::NextFactory(void) const
{
    return nextTorcI2CDeviceFactory;
}

TorcI2CBus* TorcI2CBus::gTorcI2CBus = new TorcI2CBus();

TorcI2CBus::TorcI2CBus()
  : TorcDeviceHandler()
{
}

TorcI2CBus::~TorcI2CBus()
{
    Destroy();
}

void TorcI2CBus::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    QVariantMap::const_iterator i = Details.constBegin();
    for ( ; i != Details.constEnd(); i++)
    {
        // I2C devices can be <sensors> or <outputs> (and both at the same time)
        if (i.key() != SENSORS_DIRECTORY && i.key() != OUTPUTS_DIRECTORY)
            continue;

        QVariantMap i2c = i.value().toMap();
        QVariantMap::iterator it = i2c.begin();
        for ( ; it != i2c.end(); ++it)
        {
            // look for <i2c>
            if (it.key() != I2C)
                continue;

            QVariantMap devices = it.value().toMap();
            QVariantMap::iterator it2 = devices.begin();
            for ( ; it2 != devices.end(); ++it2)
            {
                QVariantMap details = it2.value().toMap();

                // device needs an address
                if (!details.contains("i2caddress"))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("I2C device '%1' needs <i2caddress>").arg(it2.key()));
                    continue;
                }

                bool ok = false;
                // N.B. assumes hexadecimal 0xXX
                int address = details.value("i2caddress").toString().toInt(&ok, 16);
                if (!ok)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse I2C address from '%1' for device '%2'")
                        .arg(details.value("address").toString()).arg(it2.key()));
                    continue;
                }

                // TODO - handle defining the same device in sensors and outputs
                if (m_devices.contains(address))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("I2C bus already contains device at 0x%1 - ignoring").arg(address, 0, 16));
                    continue;
                }

                TorcI2CDevice* device = NULL;
                TorcI2CDeviceFactory* factory = TorcI2CDeviceFactory::GetTorcI2CDeviceFactory();
                for ( ; factory; factory = factory->NextFactory())
                {
                    device = factory->Create(address, it2.key(), details);
                    if (device)
                        break;
                }

                if (device)
                    m_devices.insert(address, device);
                else
                    LOG(VB_GENERAL, LOG_ERR, QString("Unable to find handler for I2C device '%1'").arg(it2.key()));
            }
        }
    }
}

void TorcI2CBus::Destroy(void)
{
    QMutexLocker locker(m_lock);

    qDeleteAll(m_devices);
    m_devices.clear();
}
static const QString i2cOutputTypes =
"<xs:simpleType name='pca9685ChannelNumberType'>\r\n"
"  <xs:restriction base='xs:integer'>\r\n"
"    <xs:minInclusive value='0'/>\r\n"
"    <xs:maxInclusive value='15'/>\r\n"
"  </xs:restriction>\r\n"
"</xs:simpleType>\r\n"
"\r\n"
"<xs:complexType name='pca9685ChannelType'>\r\n"
"  <xs:all>\r\n"
"    <xs:element name='name'     type='deviceNameType'/>\r\n"
"    <xs:element name='username' type='userNameType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='userdescription' type='userDescriptionType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='number'   type='pca9685ChannelNumberType'/>\r\n"
"  </xs:all>\r\n"
"</xs:complexType>\r\n"
"\r\n"
"<xs:complexType name='pca9685Type'>\r\n"
"  <xs:sequence>\r\n"
"    <!-- NB this enforces the address first -->\r\n"
"    <xs:element name='i2caddress' type='hexType' minOccurs='1' maxOccurs='1'/>\r\n"
"    <xs:element name='channel'    type='pca9685ChannelType' minOccurs='1' maxOccurs='16'/>\r\n"
"  </xs:sequence>\r\n"
"</xs:complexType>\r\n"
"\r\n"
"<xs:complexType name='i2cType'>\r\n"
"  <xs:choice minOccurs='1' maxOccurs='unbounded'>\r\n"
"    <xs:element name='pca9685' type='pca9685Type'/>\r\n"
"  </xs:choice>\r\n"
"</xs:complexType>\r\n";

static const QString i2cOutputs =
"    <xs:element minOccurs='0' maxOccurs='1' name='i2c'     type='i2cType'/>\r\n";

static const QString i2cUnique =
"    <!-- enforce unique i2c addresses -->\r\n"
"    <xs:unique name='uniqueI2CAddress'>\r\n"
"      <xs:selector xpath='.//i2caddress' />\r\n"
"      <xs:field xpath='.' />\r\n"
"    </xs:unique>\r\n";


class TorcI2CXSDFactory : public TorcXSDFactory
{
  public:
    void GetXSD(QMultiMap<QString,QString> &XSD) {
        XSD.insert(XSD_OUTPUTTYPES, i2cOutputTypes);
        XSD.insert(XSD_OUTPUTS, i2cOutputs);
        XSD.insert(XSD_UNIQUE, i2cUnique);
    }
} TorcI2CXSDFactory;
