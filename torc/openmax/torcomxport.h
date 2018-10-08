#ifndef TORCOMXPORT_H
#define TORCOMXPORT_H

// Qt
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

// Torc
#include "torcomxcore.h"
#include "torcomxcomponent.h"

// OpenMaxIL
#include "OMX_Core.h"
#include "OMX_Component.h"

class TorcOMXPort Q_DECL_FINAL
{
  public:
    TorcOMXPort(TorcOMXComponent *Parent, OMX_HANDLETYPE Handle, OMX_U32 Port, OMX_INDEXTYPE Domain);
   ~TorcOMXPort();

    OMX_U32                       GetPort        (void);
    OMX_INDEXTYPE                 GetDomain      (void);
    OMX_ERRORTYPE                 EnablePort     (bool Enable, bool Wait = true);
    OMX_U32                       GetAvailableBuffers (void);
    OMX_ERRORTYPE                 CreateBuffers  (void);
    OMX_ERRORTYPE                 DestroyBuffers (void);
    OMX_ERRORTYPE                 Flush          (void);
    OMX_ERRORTYPE                 MakeAvailable  (OMX_BUFFERHEADERTYPE* Buffer);
    OMX_BUFFERHEADERTYPE*         GetBuffer      (OMX_S32 Timeout);

  private:
    TorcOMXComponent             *m_parent;
    OMX_HANDLETYPE                m_handle;
    OMX_U32                       m_port;
    OMX_INDEXTYPE                 m_domain;
    QMutex                        m_lock;
    QWaitCondition                m_wait;
    QList<OMX_BUFFERHEADERTYPE*>  m_buffers;
    QQueue<OMX_BUFFERHEADERTYPE*> m_availableBuffers;
    OMX_U32                       m_alignment;

  private:
    Q_DISABLE_COPY(TorcOMXPort)
};

#endif // TORCOMXPORT_H
