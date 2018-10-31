#ifndef TESTTORCLOCALCONTEXT_H
#define TESTTORCLOCALCONTEXT_H

#include <QObject>

class TestTorcLocalContext : public QObject
{
    Q_OBJECT

  public:
    TestTorcLocalContext(int argc, char **argv)
      : mArgc(argc),
        mArgv(argv)
    {
    }

  private slots:
    void testTorcLocalContext(void);

  private:
    Q_DISABLE_COPY(TestTorcLocalContext)
    int    mArgc;
    char **mArgv;
};

#endif // TESTTORCLOCALCONTEXT_H
