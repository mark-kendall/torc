#ifndef TORCPOWEROSX_H
#define TORCPOWEROSX_H

// OS X
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

// Torc
#include "torcpower.h"

class TorcPowerOSX final : public TorcPower
{
    Q_OBJECT

  public:
    TorcPowerOSX();
    virtual ~TorcPowerOSX();

    bool DoShutdown        (void) override;
    bool DoSuspend         (void) override;
    bool DoHibernate       (void) override;
    bool DoRestart         (void) override;
    void Refresh           (void);

  protected:
    static void PowerCallBack       (void *Reference, io_service_t Service,
                                     natural_t Type, void *Data);
    static void PowerSourceCallBack (void *Reference);

  private:
    CFRunLoopSourceRef    m_powerRef;
    io_connect_t          m_rootPowerDomain;
    io_object_t           m_powerNotifier;
    IONotificationPortRef m_powerNotifyPort;
};

#endif // TORCPOWEROSX_H
