// Qt
#include <QtTest/QtTest>
#include <QJsonDocument>

// Torc
#include "torclocalcontext.h"
#include "testtorclocalcontext.h"
#include "torcinputs.h"
#include "torcoutputs.h"
#include "torccontrols.h"

// std
#include <signal.h>

void TestTorcLocalContext::testTorcLocalContext(void)
{
    TorcCommandLine *cmdl = new TorcCommandLine(TorcCommandLine::Database | TorcCommandLine::LogFile | TorcCommandLine::XSDTest);
    bool exit = false;
    cmdl->Evaluate(mArgc, mArgv, exit);
    if (!exit)
    {
        TorcLocalContext::Create(cmdl);
        // give the admin loop time to process - otherwise we try to delete the local context
        // while it is still being referenced
        usleep(2000000);

        // actually test something
        QVERIFY(!TorcInputs::gInputs->GetInputList().isEmpty());
        QVERIFY(!TorcInputs::gInputs->GetInputTypes().isEmpty());
        QVERIFY(!TorcOutputs::gOutputs->GetOutputList().isEmpty());
        QVERIFY(!TorcOutputs::gOutputs->GetOutputTypes().isEmpty());
        QVERIFY(!TorcControls::gControls->GetControlList().isEmpty());
        QVERIFY(!TorcControls::gControls->GetControlTypes().isEmpty());

        kill(getpid(), SIGINT);
        TorcLocalContext::TearDown();
    }
    delete cmdl;
}

