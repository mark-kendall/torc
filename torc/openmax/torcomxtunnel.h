#ifndef TORCOMXTUNNEL_H
#define TORCOMXTUNNEL_H

// Qt
#include <QMutex>

// Torc
#include "torcomxcore.h"
#include "torcomxport.h"
#include "torcomxcomponent.h"

// OpenMaxIL
#include "OMX_Core.h"
#include "OMX_Component.h"

class TorcOMXTunnel Q_DECL_FINAL
{
  public:
    TorcOMXTunnel(TorcOMXCore *Core, TorcOMXComponent *Source, OMX_U32 SourceIndex, OMX_INDEXTYPE SourceDomain,
                  TorcOMXComponent *Destination, OMX_U32 DestinationIndex, OMX_INDEXTYPE DestinationDomain);
   ~TorcOMXTunnel();

    bool              IsConnected (void);
    OMX_ERRORTYPE     Flush       (void);
    OMX_ERRORTYPE     Create      (void);
    OMX_ERRORTYPE     Destroy     (void);

  private:
    bool              m_connected;
    TorcOMXCore      *m_core;
    QMutex            m_lock;
    TorcOMXComponent *m_source;
    OMX_U32           m_sourceIndex;
    OMX_INDEXTYPE     m_sourceDomain;
    OMX_U32           m_sourcePort;
    TorcOMXComponent *m_destination;
    OMX_U32           m_destinationIndex;
    OMX_INDEXTYPE     m_destinationDomain;
    OMX_U32           m_destinationPort;

private:
  Q_DISABLE_COPY(TorcOMXTunnel)
};

#endif // TORCOMXTUNNEL_H
