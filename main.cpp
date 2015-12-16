// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "torccommandline.h"

int main(int argc, char **argv)
{
    int ret = TORC_EXIT_OK;

    do
    {
        QCoreApplication torc(argc, argv);
        QCoreApplication::setApplicationName("torc");
        QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

        {
            bool justexit = false;
            QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::Database | TorcCommandLine::LogFile));

            if (!cmdline.data())
                return TORC_EXIT_UNKOWN_ERROR;

            ret = cmdline->Evaluate(argc, argv, justexit);

            if (ret != TORC_EXIT_OK)
                return ret;

            if (justexit)
                return ret;

            if (int error = TorcLocalContext::Create(cmdline.data()))
                return error;
        }

        ret = qApp->exec();
        TorcLocalContext::TearDown();

    } while (ret == TORC_EXIT_RESTART);

    return ret;
}

