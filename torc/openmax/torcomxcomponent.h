#ifndef TORCOMXCOMPONENT_H
#define TORCOMXCOMPONENT_H

// Qt
#include <QMutex>
#include <QWaitCondition>

// Torc
#include "torcomxcore.h"

// OpenMaxIL
#include "OMX_Core.h"
#include "OMX_Component.h"

class TorcOMXPort;

class TorcOMXEvent
{
  public:
    TorcOMXEvent(OMX_EVENTTYPE Type, OMX_U32 Data1, OMX_U32 Data2);

    OMX_EVENTTYPE m_type;
    OMX_U32       m_data1;
    OMX_U32       m_data2;
};

class TorcOMXComponent final
{
  public:
    static OMX_ERRORTYPE    EventHandlerCallback    (OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData);
    static OMX_ERRORTYPE    EmptyBufferDoneCallback (OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer);
    static OMX_ERRORTYPE    FillBufferDoneCallback  (OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer);

  public:
    explicit TorcOMXComponent(OMX_STRING Component);
   ~TorcOMXComponent();

    bool                    IsValid                 (void);
    QString                 GetName                 (void);
    OMX_HANDLETYPE          GetHandle               (void);
    OMX_ERRORTYPE           SetState                (OMX_STATETYPE State, bool Wait = true);
    OMX_STATETYPE           GetState                (void);
    OMX_ERRORTYPE           SetParameter            (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_ERRORTYPE           GetParameter            (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_ERRORTYPE           SetConfig               (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_ERRORTYPE           GetConfig               (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_U32                 GetPort                 (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain);
    OMX_ERRORTYPE           EnablePort              (OMX_DIRTYPE Direction, OMX_U32 Index, bool Enable, OMX_INDEXTYPE Domain, bool Wait = true);
    OMX_ERRORTYPE           DisablePorts            (OMX_INDEXTYPE Domain);
    OMX_U32                 GetAvailableBuffers     (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain);
    OMX_U32                 GetInUseBuffers         (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain);
    OMX_ERRORTYPE           EmptyThisBuffer         (OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           FillThisBuffer          (OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           CreateBuffers           (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain, QObject* Owner = nullptr);
    OMX_ERRORTYPE           DestroyBuffers          (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain);
    OMX_BUFFERHEADERTYPE*   GetBuffer               (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_U32 Timeout, OMX_INDEXTYPE Domain);
    OMX_ERRORTYPE           FlushBuffer             (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain);
    OMX_ERRORTYPE           EventHandler            (OMX_HANDLETYPE Component, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData);
    OMX_ERRORTYPE           EmptyBufferDone         (OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           FillBufferDone          (OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           SendCommand             (OMX_COMMANDTYPE Command, OMX_U32 Parameter, OMX_PTR Data);
    OMX_ERRORTYPE           WaitForResponse         (OMX_U32 Command, OMX_U32 Data2, OMX_S32 Timeout);

  protected:
    void                    AnalysePorts            (OMX_INDEXTYPE Domain);
    TorcOMXPort*            FindPort                (OMX_DIRTYPE Direction, OMX_U32 Index, OMX_INDEXTYPE Domain);

  protected:
    bool                    m_valid;
    OMX_HANDLETYPE          m_handle;
    QMutex                  m_lock;
    QString                 m_componentName;
    QList<TorcOMXPort*>     m_inputPorts;
    QList<TorcOMXPort*>     m_outputPorts;
    QList<TorcOMXEvent>     m_eventQueue;
    QMutex                  m_eventQueueLock;
    QWaitCondition          m_eventQueueWait;

  private:
    Q_DISABLE_COPY(TorcOMXComponent)
};

#endif

