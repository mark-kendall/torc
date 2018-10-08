#ifndef TORCOMXCORE_H
#define TORCOMXCORE_H

// Qt
#include <QLibrary>

// OpenMaxIL
#ifndef OMX_SKIP64BIT
#define OMX_SKIP64BIT
#endif

#include "OMX_Types.h"
#include "OMX_Core.h"

#ifndef OMX_VERSION_MAJOR
#define OMX_VERSION_MAJOR 1
#endif
#ifndef OMX_VERSION_MINOR
#define OMX_VERSION_MINOR 1
#endif
#ifndef OMX_VERSION_REVISION
#define OMX_VERSION_REVISION 2
#endif
#ifndef OMX_VERSION_STEP
#define OMX_VERSION_STEP 0
#endif

#define OMX_INITSTRUCTURE(Struct) \
memset(&(Struct), 0, sizeof((Struct))); \
(Struct).nSize = sizeof((Struct)); \
(Struct).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
(Struct).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
(Struct).nVersion.s.nRevision     = OMX_VERSION_REVISION; \
(Struct).nVersion.s.nStep         = OMX_VERSION_STEP;

QString EventToString   (OMX_EVENTTYPE Event);
QString StateToString   (OMX_STATETYPE State);
QString ErrorToString   (OMX_ERRORTYPE Error);
QString CommandToString (OMX_COMMANDTYPE Command);
QString DomainToString  (OMX_INDEXTYPE Domain);

#define OMX_ERROR(Error, Component, Message) \
    LOG(VB_GENERAL, LOG_ERR, QString("%1: %2 (Error '%3')").arg(Component).arg(Message).arg(ErrorToString(Error)));
#define OMX_CHECK(Error, Component, Message) \
    if (OMX_ErrorNone != Error) { OMX_ERROR(Error, Component, Message); return Error; }
#define OMX_CHECKX(Error, Component, Message) \
    if (OMX_ErrorNone != Error) { OMX_ERROR(Error, Component, Message); }

class TorcOMXCore
{
  public:
    TorcOMXCore();
   ~TorcOMXCore();
};

#endif // TORCOMXCORE_H
