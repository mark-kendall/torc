/*
* Copyright (C) Mark Kendall 2012-18
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/// Qt

#include <QCoreApplication>
#include <QThread>

// Torc
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "torccommandline.h"
#include "torcxsdtest.h"

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
            QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::Database | TorcCommandLine::LogFile | TorcCommandLine::XSDTest));

            if (!cmdline.data())
                return TORC_EXIT_UNKOWN_ERROR;

            ret = cmdline->Evaluate(argc, argv, justexit);

            if (ret != TORC_EXIT_OK)
                return ret;

            if (justexit)
                return ret;

            if (!(cmdline.data()->GetValue("xsdtest").toString().isEmpty()))
                return TorcXSDTest::RunXSDTestSuite(cmdline.data());

            if (int error = TorcLocalContext::Create(cmdline.data()))
                return error;
        }

        ret = qApp->exec();
        TorcLocalContext::TearDown();

    } while (ret == TORC_EXIT_RESTART);

    return ret;
}

