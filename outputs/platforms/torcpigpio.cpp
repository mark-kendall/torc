/* Class TorcPiGPIO
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
#include "torccentral.h"
#include "torcpigpio.h"
#include "torcinputs.h"
#include "torcoutputs.h"

// wiringPi
#include "wiringPi.h"

TorcPiGPIO* TorcPiGPIO::gPiGPIO = new TorcPiGPIO();

TorcPiGPIO::TorcPiGPIO()
  : m_setup(false)
{
    if (wiringPiSetup() > -1)
        m_setup = true;
}

TorcPiGPIO::~TorcPiGPIO()
{
}

void TorcPiGPIO::Create(const QVariantMap &GPIO)
{
    QMutexLocker locker(m_lock);

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;

        if (!m_setup)
        {
            // NB wiringPi will have terminated the program already!
            LOG(VB_GENERAL, LOG_ERR, "wiringPi is not initialised");
        }
        else
        {
            int revision = piBoardRev();
            if (revision == 1)
                LOG(VB_GENERAL, LOG_INFO, "Revision 1 board - 7 pins available (0-6)");
            else if (revision == 2)
                LOG(VB_GENERAL, LOG_INFO, "Revision 2 board - 22 pins available (0-6 and 17-31");
            else
                LOG(VB_GENERAL, LOG_INFO, "Unknown board revision...");
        }
    }

    QVariantMap::const_iterator i = GPIO.constBegin();
    for ( ; i != GPIO.constEnd(); i++)
    {
        // GPIO can be under <sensors> or <outputs>
        if (i.key() != SENSORS_DIRECTORY && i.key() != OUTPUTS_DIRECTORY)
            continue;

        bool output = i.key() == OUTPUTS_DIRECTORY;

        QVariantMap gpio = i.value().toMap();
        QVariantMap::const_iterator it = gpio.begin();
        for ( ; it != gpio.end(); ++it)
        {
            // and find gpio
            if (it.key() == PI_GPIO)
            {
                // and find pins
                QVariantMap pins = it.value().toMap();
                QVariantMap::const_iterator it2 = pins.constBegin();
                for ( ; it2 != pins.constEnd(); ++it2)
                {
                    QString    type = it2.key();
                    QVariantMap pin = it2.value().toMap();

                    if (!pin.contains("gpiopinnumber"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("GPIO device '%1' does not specify pin <number>").arg(pin.value("name").toString()));
                        continue;
                    }

                    if (!pin.contains("default") && output)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("GPIO device '%1' does not specify <default> value").arg(pin.value("name").toString()));
                        continue;
                    }

                    bool ok = false;
                    int number = pin.value("gpiopinnumber").toInt(&ok);
                    if (!ok || (ok && (wpiPinToGpio(number) < 0)))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse valid pin from '%1'").arg(pin.value("pin").toString()));
                        continue;
                    }

                    if (m_inputs.contains(number) || m_outputs.contains(number) || m_pwmOutputs.contains(number))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("GPIO Pin #%1 is already in use").arg(number));
                        continue;
                    }

                    if (type == "switch")
                    {
                        if (output)
                        {
                            TorcPiSwitchOutput* out = new TorcPiSwitchOutput(number, pin);
                            m_outputs.insert(number, out);
                        }
                        else
                        {
                            TorcPiSwitchInput* in   = new TorcPiSwitchInput(number, pin);
                            m_inputs.insert(number, in);
                        }
                    }
                    else if (type == "pwm" && output)
                    {
                        TorcPiPWMOutput* out = new TorcPiPWMOutput(number, pin);
                        m_pwmOutputs.insert(number, out);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, "Unknown GPIO device type");
                    }
                }
            }
        }
    }
}

void TorcPiGPIO::Destroy(void)
{
    QMutexLocker locker(m_lock);

    QMap<int,TorcPiSwitchInput*>::iterator it = m_inputs.begin();
    for ( ; it != m_inputs.end(); ++it)
    {
         TorcInputs::gInputs->RemoveInput(it.value());
         it.value()->DownRef();
    }
    m_inputs.clear();

    QMap<int,TorcPiSwitchOutput*>::iterator it2 = m_outputs.begin();
    for ( ; it2 != m_outputs.end(); ++it2)
    {
         TorcOutputs::gOutputs->RemoveOutput(it2.value());
         it2.value()->DownRef();
    }
    m_outputs.clear();

    QMap<int,TorcPiPWMOutput*>::iterator it3 = m_pwmOutputs.begin();
    for ( ; it3 != m_pwmOutputs.end(); ++it3)
    {
         TorcOutputs::gOutputs->RemoveOutput(it3.value());
         it3.value()->DownRef();
    }
    m_pwmOutputs.clear();
}

/* For revision 1 (original Pi Model b) boards, allow wiringPi pins 0 to 6.
 * Pin 7 is used by the kernel for the 1Wire bus.
 * Pins 8 and 9 are for I2C.
 * Pins 10-14 are for SPI.
 * Pins 15 and 16 are for UART by default.
*/
static const QString gpioPinNumberTypeRev1 =
"<xs:simpleType name='gpioPinNumberType'>\r\n"
"  <xs:restriction base='xs:integer'>\r\n"
"    <xs:minInclusive value='0'/>\r\n"
"    <xs:maxInclusive value='6'/>\r\n"
"  </xs:restriction>\r\n"
"</xs:simpleType>\r\n";

/* For revision 2 boards and beyond, we have the Rev 1 pins as above, plus
 * pins 17-20 on Model B Rev 2 boards only and 21-31 for B+...
*/
static const QString gpioPinNumberTypeRev2 =
"<xs:simpleType name='gpioPinNumberType'>\r\n"
"  <xs:restriction base='xs:integer'>\r\n"
"    <xs:enumeration value='0'/>\r\n"
"    <xs:enumeration value='1'/>\r\n"
"    <xs:enumeration value='2'/>\r\n"
"    <xs:enumeration value='3'/>\r\n"
"    <xs:enumeration value='4'/>\r\n"
"    <xs:enumeration value='5'/>\r\n"
"    <xs:enumeration value='6'/>\r\n"
"    <xs:enumeration value='17'/>\r\n"
"    <xs:enumeration value='18'/>\r\n"
"    <xs:enumeration value='19'/>\r\n"
"    <xs:enumeration value='20'/>\r\n"
"    <xs:enumeration value='21'/>\r\n"
"    <xs:enumeration value='22'/>\r\n"
"    <xs:enumeration value='23'/>\r\n"
"    <xs:enumeration value='24'/>\r\n"
"    <xs:enumeration value='25'/>\r\n"
"    <xs:enumeration value='26'/>\r\n"
"    <xs:enumeration value='27'/>\r\n"
"    <xs:enumeration value='28'/>\r\n"
"    <xs:enumeration value='29'/>\r\n"
"    <xs:enumeration value='30'/>\r\n"
"    <xs:enumeration value='31'/>\r\n"
"  </xs:restriction>\r\n"
"</xs:simpleType>\r\n";

static const QString pigpioInputTypes =
"\r\n"
"<xs:complexType name='gpioInputSwitchType'>\r\n"
"  <xs:all>\r\n"
"    <xs:element name='name'     type='deviceNameType'/>\r\n"
"    <xs:element name='username' type='userNameType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='userdescription' type='userDescriptionType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='gpiopinnumber'   type='gpioPinNumberType'/>\r\n"
"  </xs:all>\r\n"
"</xs:complexType>\r\n"
"\r\n"
"<xs:complexType name='gpioInputType'>\r\n"
"  <xs:sequence>\r\n"
"    <xs:element minOccurs='1' maxOccurs='unbounded' name='switch' type='gpioInputSwitchType'/>\r\n"
"  </xs:sequence>\r\n"
"</xs:complexType>\r\n";

static const QString pigpioInputs =
"    <xs:element minOccurs='0' maxOccurs='1' name='gpio'    type='gpioInputType'/>\r\n";

static const QString pigpioOutputTypes =
"<xs:complexType name='gpioOutputSwitchType'>\r\n"
"  <xs:all>\r\n"
"    <xs:element name='name'     type='deviceNameType'/>\r\n"
"    <xs:element name='username' type='userNameType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='userdescription' type='userDescriptionType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='gpiopinnumber' type='gpioPinNumberType'/>\r\n"
"    <xs:element name='default'  type='switchNumberType'/>\r\n"
"  </xs:all>\r\n"
"</xs:complexType>\r\n"
"\r\n"
"<xs:complexType name='gpioOutputPWMType'>\r\n"
"  <xs:all>\r\n"
"    <xs:element name='name'     type='deviceNameType'/>\r\n"
"    <xs:element name='username' type='userNameType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='userdescription' type='userDescriptionType' minOccurs='0' maxOccurs='1'/>\r\n"
"    <xs:element name='gpiopinnumber' type='gpioPinNumberType'/>\r\n"
"    <xs:element name='default'  type='pwmNumberType'/>\r\n"
"  </xs:all>\r\n"
"</xs:complexType>\r\n"
"\r\n"
"<xs:complexType name='gpioOutputType'>\r\n"
"  <xs:choice minOccurs='1' maxOccurs='unbounded'>\r\n"
"    <xs:element name='switch' type='gpioOutputSwitchType'/>\r\n"
"    <xs:element name='pwm'    type='gpioOutputPWMType'/>\r\n"
"  </xs:choice>\r\n"
"</xs:complexType>\r\n";

static const QString pigpioOutputs =
"    <xs:element minOccurs='0' maxOccurs='1' name='gpio'    type='gpioOutputType'/>\r\n";

static const QString pigpioUnique =
"  <!-- enforce unique GPIO pin numbers -->\r\n"
"  <xs:unique name='uniqueGPIOPinNumber'>\r\n"
"    <xs:selector xpath='.//gpiopinnumber' />\r\n"
"    <xs:field xpath='.' />\r\n"
"  </xs:unique>\r\n";

class TorcPiGPIOXSDFactory : public TorcXSDFactory
{
  public:
    TorcPiGPIOXSDFactory() : TorcXSDFactory()
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
        QCoreApplication::setSetuidAllowed(true);
#endif
    }

    void GetXSD(QMultiMap<QString,QString> &XSD) {
        bool rev1 = piBoardRev() == 1;
        XSD.insert(XSD_INPUTTYPES, (rev1 ? gpioPinNumberTypeRev1 : gpioPinNumberTypeRev2) + pigpioInputTypes);
        XSD.insert(XSD_INPUTS, pigpioInputs);
        XSD.insert(XSD_OUTPUTTYPES, pigpioOutputTypes);
        XSD.insert(XSD_OUTPUTS, pigpioOutputs);
        XSD.insert(XSD_UNIQUE, pigpioUnique);
    }
} TorcPiGPIOXSDFactory;

