// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "torccommandline.h"

int main(int argc, char **argv)
{
    new QCoreApplication(argc, argv);
    QCoreApplication::setApplicationName("torc");
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    int ret = GENERIC_EXIT_OK;

    {
        bool justexit = false;
        QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::Database | TorcCommandLine::LogFile));

        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        ret = cmdline->Evaluate(argc, argv, justexit);

        if (ret != GENERIC_EXIT_OK)
            return ret;

        if (justexit)
            return ret;

        if (int error = TorcLocalContext::Create(cmdline.data()))
            return error;
    }

    ret = qApp->exec();
    TorcLocalContext::TearDown();
    return ret;
}

