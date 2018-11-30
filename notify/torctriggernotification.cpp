/* Class TorcTriggerNotification
*
* Copyright (C) Mark Kendall 2016-18
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

// Qt
#include <QLocale>

// Torc
#include "torclogging.h"
#include "torcdevice.h"
#include "torcnotify.h"
#include "torctriggernotification.h"

/*! \class TorcTriggerNotification
 *
 * A notification class for sending messages when the input value switches from low to
 * high or high to low.
 *
 * A single notification is sent when the trigger occurs. If the input remains unchanged, no further notifications
 * are sent. A notification is not sent when the input changes back - this should be handled by a matching, configured
 * trigger notification. Consider some form of hysteresis for non-binary or frequently changing inputs to avoid frequent
 * notifications.
 *
 * \code
 * <torc>
 *   <notify>
 *     <notifications>
 *       <trigger>
 *         <name>test</name>
 *         <inputs><device>mytrigger</device></inputs> <-- an input or control
 *         <outputs><device>mynotifier></device></outputs> <- a  notifier
 *         <references><device>myinterestingdevice</device></references> <-- devices whose values we want to notify
 *         <message>
 *           <title>%applicationname% ALERT</title>
 *           <body>Interesting value %myinterestingdevice% at %datatime%</body>
 *         </message>
 *       </trigger>
 *     </notifications>
 *   </notify>
 * </torc>
 * \endcode
 *
 * To trigger the output when the input transitions from high to low (the default is low to high) add a <triggerlow> member.
 *
 * \code
 * <torc>
 *   <notify>
 *     <notifications>
 *       <trigger>
 *         <name>test</name>
 *         <triggerlow>yes</triggerlow> <-- !!!!
 *         <inputs><device>mytrigger</device></inputs> <-- an input or control
 *         <outputs><device>mynotifier></device></outputs> <- a  notifier
 *         <references><device>myinterestingdevice</device></references> <-- devices whose values we want to notify
 *         <message>
 *           <title>%applicationname% ALERT</title>
 *           <body>Interesting value %myinterestingdevice% at %datatime%</body>
 *         </message>
 *       </trigger>
 *     </notifications>
 *   </notify>
 * </torc>
 * \endcode
 *
 * \sa TorcNotify
 * \sa TorcNotification
 * \sa TorcNotifier
 * \sa TorcSystemNotification
*/
TorcTriggerNotification::TorcTriggerNotification(const QVariantMap &Details)
  : TorcNotification(Details),
    m_inputName(),
    m_input(nullptr),
    m_lastValue(0.0),
    m_triggerHigh(true),
    m_customData(),
    m_references(),
    m_referenceDevices()
{
    if (uniqueId.isEmpty() || m_notifierNames.isEmpty() || m_body.isEmpty())
        return;

    if (Details.contains("inputs"))
    {
        QVariantMap inputs = Details.value("inputs").toMap();
        QVariantMap::const_iterator it = inputs.constBegin();
        for ( ; it != inputs.constEnd(); ++it)
        {
            if (it.key() == "device")
            {
                m_inputName = it.value().toString().trimmed();
                m_customData.insert("inputname", m_inputName);
                m_customData.insert("name", userName);
                break;
            }
        }
    }

    if (Details.contains("triggerlow"))
    {
        m_triggerHigh = false;
        m_lastValue = 1.0;
    }

    if (Details.contains("references"))
    {
        QVariantMap references = Details.value("references").toMap();
        QVariantMap::const_iterator it = references.constBegin();
        for ( ; it != references.constEnd(); ++it)
        {
            if (it.key() == "device")
                m_references.append(it.value().toString().trimmed());
        }
    }
}

bool TorcTriggerNotification::IsKnownInput(const QString &UniqueId)
{
    QMutexLocker locker(&lock);
    return UniqueId == m_inputName;
}

QStringList TorcTriggerNotification::GetDescription(void)
{
    if (m_triggerHigh)
        return QStringList() << tr("Trigger 0 to 1");
    return QStringList() << tr("Trigger 1 to 0");
}

void TorcTriggerNotification::Graph(QByteArray *Data)
{
    if (!Data)
        return;

    if (m_input)
        Data->append(QString("    \"%2\"->\"%1\"\r\n").arg(uniqueId).arg(m_input->GetUniqueId()));

    foreach (TorcDevice *device, m_referenceDevices)
        Data->append(QString("    \"%1\"->\"%2\" [style=dashed]\r\n").arg(device->GetUniqueId()).arg(uniqueId));

    foreach (TorcNotifier* notifier, m_notifiers)
        Data->append(QString("    \"%1\"->\"%2\"\r\n").arg(uniqueId).arg(notifier->GetUniqueId()));
}

void TorcTriggerNotification::InputValueChanged(double Value)
{
    // N.B. this should be thread safe as InputValueChanged is always triggered via a signal
    TorcDevice* input = qobject_cast<TorcDevice*>(sender());

    if (input && (input == m_input))
    {
        if ((Value > 0.0 && m_lastValue <= 0.0 && m_triggerHigh) || // low to high
            (Value <= 0.0 && m_lastValue > 0.0 && !m_triggerHigh))  // high to low
        {
            // add 'key'-'value' data for each reference (e.g. 'tanktemperature', '27.0')
            foreach (TorcDevice* device, m_referenceDevices)
                m_customData.insert(device->GetUniqueId(), QString::number(device->GetValue()));
            QVariantMap message = TorcNotify::gNotify->SetNotificationText(m_title, m_body, m_customData);
            emit Notify(message);
        }

        m_lastValue = Value;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("TriggerNotifiction '%1' signalled from unknown input").arg(uniqueId));
    }
}

/*! \brief Finalise the notification.
 *
 *  Check for the existence of input and connect ValueChanged signal to InputValueChanged slot.
*/
bool TorcTriggerNotification::Setup(void)
{
    if (m_inputName.isEmpty())
        return false;

    // setup the input
    TorcDevice* input = nullptr;
    {
        QMutexLocker locker(gDeviceListLock);
        if (!m_inputName.isEmpty() && gDeviceList->contains(m_inputName))
            input = gDeviceList->value(m_inputName);
    }

    if (!input)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find input '%1' for trigger notification '%2'").arg(m_inputName).arg(uniqueId));
        return false;
    }

    connect(input, SIGNAL(ValueChanged(double)), this, SLOT(InputValueChanged(double)));
    m_input = input;

    // check for the existence of reference devices
    foreach (QString reference, m_references)
    {
        QMutexLocker locker(gDeviceListLock);
        if (!reference.isEmpty() && gDeviceList->contains(reference))
            m_referenceDevices.append(gDeviceList->value(reference));
    }

    return TorcNotification::Setup();
}

class TorcTriggerNotificationFactory final : public TorcNotificationFactory
{
    TorcNotification* Create(const QString &Type, const QVariantMap &Details) override
    {
        if (Type == "trigger" && Details.contains("inputs"))
            return new TorcTriggerNotification(Details);
        return nullptr;
    }
} TorcTriggerNotificationFactory;
