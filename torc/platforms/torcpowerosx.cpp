/* Class TorcPowerOSX
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
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

// OS X
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

// Torc
#include "torclocalcontext.h"
#include "torccocoa.h"
#include "torcrunlooposx.h"
#include "torcpowerosx.h"

/*! \class TorcPowerOSX
 *  \brief A power monitoring class for OS X.
 *
 * TorcPowerOSX uses the IOKit framework to monitor the system's power status.
 *
 * \sa TorcRunLoopOSX
*/

static OSStatus SendAppleEventToSystemProcess(AEEventID EventToSend);

TorcPowerOSX::TorcPowerOSX()
  : m_powerRef(nullptr),
    m_rootPowerDomain(0),
    m_powerNotifier(MACH_PORT_NULL),
    m_powerNotifyPort(nullptr)
{
    CocoaAutoReleasePool pool;

    if (!gAdminRunLoop)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("OS X callback run loop not present - aborting"));
        return;
    }

    // Register for power status updates
    m_rootPowerDomain = IORegisterForSystemPower(this, &m_powerNotifyPort, PowerCallBack, &m_powerNotifier);
    if (m_rootPowerDomain)
    {
        CFRunLoopAddSource(gAdminRunLoop,
                           IONotificationPortGetRunLoopSource(m_powerNotifyPort),
                           kCFRunLoopDefaultMode);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to setup power status callback"));
    }

    // Is there a battery?
    CFArrayRef batteryinfo = nullptr;

    if (IOPMCopyBatteryInfo(kIOMasterPortDefault, &batteryinfo) == kIOReturnSuccess)
    {
        CFRelease(batteryinfo);

        // register for notification of power source changes
        m_powerRef = IOPSNotificationCreateRunLoopSource(PowerSourceCallBack, this);
        if (m_powerRef)
        {
            CFRunLoopAddSource(gAdminRunLoop, m_powerRef, kCFRunLoopDefaultMode);
            Refresh();
        }
    }

    if (!m_powerRef)
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to setup power source callback"));

    // Set capabilities
    m_canShutdown->SetValue(QVariant((bool)true));
    m_canSuspend->SetValue(QVariant((bool)IOPMSleepEnabled()));
    m_canHibernate->SetValue(QVariant((bool)true));
    m_canRestart->SetValue(QVariant((bool)true));

    Debug();
}

TorcPowerOSX::~TorcPowerOSX()
{
    CocoaAutoReleasePool pool;

    // deregister power status change notifications
    if (gAdminRunLoop)
    {
        CFRunLoopRemoveSource(gAdminRunLoop,
                              IONotificationPortGetRunLoopSource(m_powerNotifyPort),
                              kCFRunLoopDefaultMode );
        IODeregisterForSystemPower(&m_powerNotifier);
        IOServiceClose(m_rootPowerDomain);
        IONotificationPortDestroy(m_powerNotifyPort);
    }

    // deregister power source change notifcations
    if (m_powerRef && gAdminRunLoop)
    {
        CFRunLoopRemoveSource(gAdminRunLoop, m_powerRef, kCFRunLoopDefaultMode);
        CFRelease(m_powerRef);
    }
}

///Shutdown the system.
bool TorcPowerOSX::DoShutdown(void)
{
    m_httpServiceLock.lockForWrite();
    OSStatus error = SendAppleEventToSystemProcess(kAEShutDown);
    m_httpServiceLock.unlock();

    if (noErr == error)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sent shutdown command."));
        TorcLocalContext::NotifyEvent(Torc::ShuttingDown);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to send shutdown command."));
    return false;
}

///Suspend the system.
bool TorcPowerOSX::DoSuspend(void)
{
    m_httpServiceLock.lockForWrite();
    OSStatus error = SendAppleEventToSystemProcess(kAESleep);
    m_httpServiceLock.unlock();

    if (noErr == error)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sent sleep command."));
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to send sleep command."));
    return false;
}

///Hibernate the system.
bool TorcPowerOSX::DoHibernate(void)
{
    return Suspend();
}

///Restart the system.
bool TorcPowerOSX::DoRestart(void)
{
    m_httpServiceLock.lockForWrite();
    OSStatus error = SendAppleEventToSystemProcess(kAERestart);
    m_httpServiceLock.unlock();

    if (noErr == error)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sent restart command."));
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to send restart command."));
    return false;
}

/*! \brief Update the current power supply status.
 *
 * A change to the current status is notified from PowerSourceCallback, which calls this method to
 * check for the actual change.
*/
void TorcPowerOSX::Refresh(void)
{
    if (m_powerRef)
        return;

    m_httpServiceLock.lockForWrite();

    CFTypeRef  info = IOPSCopyPowerSourcesInfo();
    CFArrayRef list = IOPSCopyPowerSourcesList(info);

    for (int i = 0; i < CFArrayGetCount(list); i++)
    {
        CFTypeRef source = CFArrayGetValueAtIndex(list, i);
        CFDictionaryRef description = IOPSGetPowerSourceDescription(info, source);

        if ((CFBooleanRef)CFDictionaryGetValue(description, CFSTR(kIOPSIsPresentKey)) == kCFBooleanFalse)
            continue;

        CFStringRef type = (CFStringRef)CFDictionaryGetValue(description, CFSTR(kIOPSTransportTypeKey));
        if (type && CFStringCompare(type, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo)
        {
            CFStringRef state = (CFStringRef)CFDictionaryGetValue(description, CFSTR(kIOPSPowerSourceStateKey));
            if (state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
            {
                m_batteryLevel = TorcPower::ACPower;
            }
            else if (state && CFStringCompare(state, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo)
            {
                int32_t current;
                int32_t max;
                CFNumberRef capacity = (CFNumberRef)CFDictionaryGetValue(description, CFSTR(kIOPSCurrentCapacityKey));
                CFNumberGetValue(capacity, kCFNumberSInt32Type, &current);
                capacity = (CFNumberRef)CFDictionaryGetValue(description, CFSTR(kIOPSMaxCapacityKey));
                CFNumberGetValue(capacity, kCFNumberSInt32Type, &max);
                m_batteryLevel = (int)(((qreal)current / ((qreal)max)) * 100.0);
            }
            else
            {
                m_batteryLevel = TorcPower::UnknownPower;
            }
        }
    }

    CFRelease(list);
    CFRelease(info);

    m_httpServiceLock.unlock();

    BatteryUpdated(m_batteryLevel);
}

/*! \brief Receive notification of power status changes.
 *
 *  The OS will call this function when the power state is about to change. We can
 *  then make the appropriate internal calls to handle any necessary state changes.
 *
 *  In the case of system requests to ask whether power changes can proceed, we always allow.
*/
void TorcPowerOSX::PowerCallBack(void *Reference, io_service_t Service,
                                 natural_t Type, void *Data)
{
    (void)Service;

    CocoaAutoReleasePool pool;
    TorcPowerOSX* power = static_cast<TorcPowerOSX*>(Reference);

    if (power)
    {
        switch (Type)
        {
            case kIOMessageCanSystemPowerOff:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                power->ShuttingDown();
                break;
            case kIOMessageCanSystemSleep:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                power->Suspending();
                break;
            case kIOMessageSystemWillPowerOff:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                power->ShuttingDown();
                break;
            case kIOMessageSystemWillRestart:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                power->Restarting();
                break;
            case kIOMessageSystemWillSleep:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                power->Suspending();
                break;
            case kIOMessageSystemHasPoweredOn:
                power->WokeUp();
                break;
        }
    }
}

/*! \brief Receive notification of changes to the power supply.
 *
 *  Changes may be a change in the battery level or switches between mains power and battery.
 *
 * \note This is currently not being called using torc-server from the command line.
*/
void TorcPowerOSX::PowerSourceCallBack(void *Reference)
{
    CocoaAutoReleasePool pool;
    TorcPowerOSX* power = static_cast<TorcPowerOSX*>(Reference);

    if (power)
        power->Refresh();
}

// see Technical Q&A QA1134
OSStatus SendAppleEventToSystemProcess(AEEventID EventToSend)
{
    AEAddressDesc targetDesc;
    static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
    AppleEvent eventReply       = { typeNull, nullptr };
    AppleEvent appleEventToSend = { typeNull, nullptr };

    OSStatus error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                                  sizeof(kPSNOfSystemProcess), &targetDesc);

    if (error != noErr)
        return error;

    error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                               kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);

    AEDisposeDesc(&targetDesc);
    if (error != noErr)
        return error;

    error = AESendMessage(&appleEventToSend, &eventReply, kAENormalPriority, kAEDefaultTimeout);

    AEDisposeDesc(&appleEventToSend);

    if (error != noErr)
        return error;

    AEDisposeDesc(&eventReply);

    return error;
}

///Create a TorcPowerOSX singleton to handle power status.
class TorcPowerFactoryOSX : public TorcPowerFactory
{
    void Score(int &Score)
    {
        if (Score <= 10)
            Score = 10;
    }

    TorcPower* Create(int Score)
    {
        if (Score <= 10)
            return new TorcPowerOSX();

        return nullptr;
    }
} TorcPowerFactoryOSX;

