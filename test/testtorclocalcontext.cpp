// Qt
#include <QtTest/QtTest>
#include <QJsonDocument>

// Torc
#include "torclocalcontext.h"
#include "testtorclocalcontext.h"

void TestTorcLocalContext::testTorcLocalContext(void)
{
    TorcCommandLine *cmdl = new TorcCommandLine(TorcCommandLine::Database | TorcCommandLine::LogFile | TorcCommandLine::XSDTest);
    bool exit = false;
    cmdl->Evaluate(mArgc, mArgv, exit);
    if (!exit)
    {
        TorcLocalContext::Create(cmdl);
        TorcLocalContext::TearDown();
    }
    delete cmdl;
}

