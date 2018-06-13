#ifndef TORCOMXTEST_H
#define TORCOMXTEST_H

// Torc
#include "torcomxcore.h"

class TorcOMXTest Q_DECL_FINAL
{
  public:
    TorcOMXTest();
   ~TorcOMXTest();

  private:
    Q_DISABLE_COPY(TorcOMXTest)
    TorcOMXCore *m_core;
};

#endif // TORCOMXTEST_H
