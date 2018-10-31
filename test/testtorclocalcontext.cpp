// Qt
#include <QtTest/QtTest>
#include <QJsonDocument>

// Torc
#include "torclocalcontext.h"
#include "testtorclocalcontext.h"
#include "torcinputs.h"
#include "torcoutputs.h"

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
        TorcReferenceCounter::EventLoopEnding(true);
        TorcLocalContext::TearDown();
    }
    delete cmdl;
}

