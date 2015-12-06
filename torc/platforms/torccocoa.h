#ifndef TORCCOCOA_H
#define TORCCOCOA_H

// Std
#import <ApplicationServices/ApplicationServices.h>

class CocoaAutoReleasePool
{
  public:
    CocoaAutoReleasePool();
   ~CocoaAutoReleasePool();

  private:
    void *m_pool;
};

#endif // TORCCOCOA_H
