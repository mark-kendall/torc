/* Class TorcXSDTest
*
* Copyright (C) Mark Kendall 2016
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
*/

// Qt
#include <QDir>
#include <QFile>

// Torc
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "torclogging.h"
#include "torcdirectories.h"
#include "torcxmlvalidator.h"
#include "torccentral.h"
#include "torcxsdtest.h"

TorcXSDTest::TorcXSDTest()
{
}

TorcXSDTest::~TorcXSDTest()
{
}

/*! \brief Perform XSD test validation of XML files.
 *
 * RunXSDTestSuite will search for XML files in the directory specified by the command line argument 'xsdtest'.
 * All files are then validated against the <i>current</i> XSD and any passes are logged.
 * \note The test XML files are expected to be invalid configurations.
*/
int TorcXSDTest::RunXSDTestSuite(TorcCommandLine *CommandLine)
{
#ifdef USING_XMLPATTERNS
    if (!CommandLine)
        return TORC_EXIT_INVALID_CMDLINE;

    if (int error = TorcLocalContext::Create(CommandLine, false))
        return error;

    QString directory = CommandLine->GetValue("xsdtest").toString();
    LOG(VB_GENERAL, LOG_INFO, QString("Starting XSD test from %1").arg(directory));

    QString basexsd    = GetTorcShareDir() + "/html/torc.xsd";
    QByteArray fullxsd = TorcCentral::GetCustomisedXSD(basexsd);
    if (fullxsd.isEmpty())
    {
        TorcLocalContext::TearDown();
        return TORC_EXIT_UNKOWN_ERROR;
    }

    QDir dir(directory);
    QStringList filters;
    filters << "*.xml";

    QStringList testfiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable | QDir::CaseSensitive, QDir::Name);

    if (testfiles.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Failed to find any xml files in %1").arg(dir.path()));
        TorcLocalContext::TearDown();
        return TORC_EXIT_UNKOWN_ERROR;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Found %1 files.").arg(testfiles.size()));
    TorcXmlValidator::gSilent = true;
    bool allfailed = true;
    foreach (QString file, testfiles)
    {
        // these should all FAIL!
        QString path = directory + "/" + file;
        TorcXmlValidator validator(path, fullxsd);
        if (validator.Validated())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Unexpected pass: %1").arg(path));
            allfailed = false;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, path);
        }
    }

    if (allfailed)
        LOG(VB_GENERAL, LOG_INFO, "All test files failed as expected");
    TorcLocalContext::TearDown();

#else
    LOG(VB_GENERAL, LOG_INFO, "QXMLPatterns not available.");
#endif

    return TORC_EXIT_OK;
}

