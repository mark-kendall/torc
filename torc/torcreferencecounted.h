#ifndef TORCREFERENCECOUNTED_H_
#define TORCREFERENCECOUNTED_H_

// Qt
#include <QAtomicInt>

class TorcReferenceCounter
{
  public:
    static void EventLoopEnding(bool Ending);

    TorcReferenceCounter(void);
    virtual ~TorcReferenceCounter(void);

    void UpRef(void);
    bool DownRef(void);
    bool IsShared(void);

  private:
    QAtomicInt   m_refCount;
    static bool  m_eventLoopEnding;
};

class TorcReferenceLocker
{
  public:
    TorcReferenceLocker(TorcReferenceCounter *Counter);
   ~TorcReferenceLocker();

  private:
    TorcReferenceCounter *m_refCountedObject;
};

#endif
