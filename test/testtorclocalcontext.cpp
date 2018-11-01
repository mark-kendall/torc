// Qt
#include <QtTest/QtTest>
#include <QJsonDocument>

// Torc
#include "torclocalcontext.h"
#include "testtorclocalcontext.h"
#include "torcinputs.h"
#include "torcoutputs.h"

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
        (void)TorcInputs::gInputs->GetInputList();
        (void)TorcInputs::gInputs->GetInputTypes();
        (void)TorcOutputs::gOutputs->GetOutputList();
        (void)TorcOutputs::gOutputs->GetOutputTypes();
        // give the admin loop time to process - otherwise we try to delete the local context
        // while it is still being referenced
        usleep(2000000);
        kill(getpid(), SIGINT);
        TorcLocalContext::TearDown();
    }
    delete cmdl;
}

