#ifndef TORCXSDTEST_H
#define TORCXSDTEST_H

// Qt
#include <QString>

// Torc
#include "torccommandline.h"
class TorcXSDTest
{
  public:
    static int RunXSDTestSuite (TorcCommandLine *CommandLine);

  private:
    TorcXSDTest() = default;
    ~TorcXSDTest() = default;
};

#endif // TORCXSDTEST_H
