#ifndef TORCREFERENCECOUNTED_H_
#define TORCREFERENCECOUNTED_H_

// Qt
#include <QAtomicInt>

class TorcReferenceCounter
{
  public:
    static void EventLoopEnding(bool Ending);

    TorcReferenceCounter(void);
    virtual ~TorcReferenceCounter(void) = default;

    void UpRef(void);
    virtual bool DownRef(void);
    bool IsShared(void);

  protected:
    QAtomicInt   m_refCount;
    static bool  m_eventLoopEnding;
};

class TorcReferenceLocker
{
  public:
    explicit TorcReferenceLocker(TorcReferenceCounter *Counter);
   ~TorcReferenceLocker();

  private:
    Q_DISABLE_COPY(TorcReferenceLocker)
    TorcReferenceCounter *m_refCountedObject;
};

#endif
