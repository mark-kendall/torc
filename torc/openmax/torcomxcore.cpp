/* Class TorcOMXCore
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013-18
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
#include "torcomxcore.h"

QString EventToString(OMX_EVENTTYPE Event)
{
    switch (Event)
    {
        case OMX_EventCmdComplete:               return QStringLiteral("CmdComplete");
        case OMX_EventError:                     return QStringLiteral("Error");
        case OMX_EventMark:                      return QStringLiteral("Mark");
        case OMX_EventPortSettingsChanged:       return QStringLiteral("PortSettingsChanged");
        case OMX_EventBufferFlag:                return QStringLiteral("BufferFlag");
        case OMX_EventResourcesAcquired:         return QStringLiteral("ResourcesAcquired");
        case OMX_EventComponentResumed:          return QStringLiteral("ComponentResumed");
        case OMX_EventDynamicResourcesAvailable: return QStringLiteral("DynamicResourcesAvailable");
        case OMX_EventPortFormatDetected:        return QStringLiteral("PortFormatDetected");
        default: break;
    }

    return QStringLiteral("Unknown %1").arg(Event);
}

QString StateToString(OMX_STATETYPE State)
{
    switch (State)
    {
        case OMX_StateInvalid:          return QStringLiteral("Invalid");
        case OMX_StateLoaded:           return QStringLiteral("Loaded");
        case OMX_StateIdle:             return QStringLiteral("Idle");
        case OMX_StateExecuting:        return QStringLiteral("Executing");
        case OMX_StatePause:            return QStringLiteral("Pause");
        case OMX_StateWaitForResources: return QStringLiteral("WaitForResources");
        default: break;
    }
    return QStringLiteral("Unknown %1").arg(State);
}

QString ErrorToString(OMX_ERRORTYPE Error)
{
    switch (Error)
    {
        case OMX_ErrorNone:                               return QStringLiteral("None");
        case OMX_ErrorInsufficientResources:              return QStringLiteral("InsufficientResources");
        case OMX_ErrorUndefined:                          return QStringLiteral("Undefined");
        case OMX_ErrorInvalidComponentName:               return QStringLiteral("InvalidComponentName");
        case OMX_ErrorComponentNotFound:                  return QStringLiteral("ComponentNotFound");
        case OMX_ErrorInvalidComponent:                   return QStringLiteral("InvalidComponent");
        case OMX_ErrorBadParameter:                       return QStringLiteral("BadParameter");
        case OMX_ErrorNotImplemented:                     return QStringLiteral("NotImplemented");
        case OMX_ErrorUnderflow:                          return QStringLiteral("Underflow");
        case OMX_ErrorOverflow:                           return QStringLiteral("Overflow");
        case OMX_ErrorHardware:                           return QStringLiteral("Hardware");
        case OMX_ErrorInvalidState:                       return QStringLiteral("InvalidState");
        case OMX_ErrorStreamCorrupt:                      return QStringLiteral("StreamCorrupt");
        case OMX_ErrorPortsNotCompatible:                 return QStringLiteral("PortsNotCompatible");
        case OMX_ErrorResourcesLost:                      return QStringLiteral("ResourcesLost");
        case OMX_ErrorNoMore:                             return QStringLiteral("NoMore");
        case OMX_ErrorVersionMismatch:                    return QStringLiteral("VersionMismatch");
        case OMX_ErrorNotReady:                           return QStringLiteral("NotReady");
        case OMX_ErrorTimeout:                            return QStringLiteral("Timeout");
        case OMX_ErrorSameState:                          return QStringLiteral("SameState");
        case OMX_ErrorResourcesPreempted:                 return QStringLiteral("ResourcesPreempted");
        case OMX_ErrorPortUnresponsiveDuringAllocation:   return QStringLiteral("PortUnresponsiveDuringAllocation");
        case OMX_ErrorPortUnresponsiveDuringDeallocation: return QStringLiteral("PortUnresponsiveDuringDeallocation");
        case OMX_ErrorPortUnresponsiveDuringStop:         return QStringLiteral("PortUnresponsiveDuringStop");
        case OMX_ErrorIncorrectStateTransition:           return QStringLiteral("IncorrectStateTransition");
        case OMX_ErrorIncorrectStateOperation:            return QStringLiteral("IncorrectStateOperation");
        case OMX_ErrorUnsupportedSetting:                 return QStringLiteral("UnsupportedSetting");
        case OMX_ErrorUnsupportedIndex:                   return QStringLiteral("UnsupportedIndex");
        case OMX_ErrorBadPortIndex:                       return QStringLiteral("BadPortIndex");
        case OMX_ErrorPortUnpopulated:                    return QStringLiteral("PortUnpopulated");
        case OMX_ErrorComponentSuspended:                 return QStringLiteral("ComponentSuspended");
        case OMX_ErrorDynamicResourcesUnavailable:        return QStringLiteral("DynamicResourcesUnavailable");
        case OMX_ErrorMbErrorsInFrame:                    return QStringLiteral("MbErrorsInFrame");
        case OMX_ErrorFormatNotDetected:                  return QStringLiteral("FormatNotDetected");
        case OMX_ErrorContentPipeOpenFailed:              return QStringLiteral("ContentPipeOpenFailed");
        case OMX_ErrorContentPipeCreationFailed:          return QStringLiteral("ContentPipeCreationFailed");
        case OMX_ErrorSeperateTablesUsed:                 return QStringLiteral("SeperateTablesUsed");
        case OMX_ErrorTunnelingUnsupported:               return QStringLiteral("TunnelingUnsupported");
        case OMX_ErrorMax:                                return QStringLiteral("Max");
        default: break;
    }

    return QStringLiteral("Unknown error 0x%1").arg(Error, 0, 16);
}

QString CommandToString(OMX_COMMANDTYPE Command)
{
    switch (Command)
    {
        case OMX_CommandStateSet:    return QStringLiteral("StateSet");
        case OMX_CommandFlush:       return QStringLiteral("Flush");
        case OMX_CommandPortDisable: return QStringLiteral("PortDisable");
        case OMX_CommandPortEnable:  return QStringLiteral("PortEnable");
        case OMX_CommandMarkBuffer:  return QStringLiteral("MarkBuffer");
        case OMX_CommandMax:         return QStringLiteral("Max");
        default: break;
    }

    return QStringLiteral("Unknown %1").arg(Command);
}

QString DomainToString(OMX_INDEXTYPE Domain)
{
    switch (Domain)
    {
        case OMX_IndexParamAudioInit: return QStringLiteral("Audio");
        case OMX_IndexParamImageInit: return QStringLiteral("Image");
        case OMX_IndexParamVideoInit: return QStringLiteral("Video");
        case OMX_IndexParamOtherInit: return QStringLiteral("Other");
        default: break;
    }

    return QStringLiteral("Unknown");
}

TorcOMXCore::TorcOMXCore()
{
    OMX_ERRORTYPE error = OMX_Init();
    if (OMX_ErrorNone != error)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to initialise OMXCore (Error '%1')").arg(ErrorToString(error)));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Available OMX components:"));
    OMX_U32 index = 0;
    char componentname[128];
    while (OMX_ComponentNameEnum(&componentname[0], 128, index++) == OMX_ErrorNone)
        LOG(VB_GENERAL, LOG_INFO, QString(componentname));
}

TorcOMXCore::~TorcOMXCore()
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Closing OpenMax Core"));
    OMX_Deinit();
}
