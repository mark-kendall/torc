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

/*! \page xsd Configuration file format
 * \section xsdintro Introduction
 * Torc is configured via an XML file. By design, the structure and format of this file is highly specific
 * - this is to ensure that it operates exactly as the user intended. Subtle (and not so subtle) errors should
 * not lead to unexpected behaviour.
 *
 * \section xsdgeneral General XML notes
 * -# All XML definitions are case sensitive.
 *
 * \section xsdxsd Device specification
 * All devices require a '<name>' that uniquely identifies that device.
 * To that end, a name must be unique, must not be empty and for internal purposes can only contain
 * alphanumeric characters (a-z, A-Z and 0-9) as well as '_' and '-',
 *
 * Devices may also have '<username>' and '<userdescription>' tags. These do not affect the operation of
 * the device and are used purely for a user friendly interface.
 *
 * \section xsdinputs Input devices
 * As well as the default device specifications, an input device <b>must</b> specify a default value and
 * any type specific values (e.g. gpiopinnumber for a GPIO input).
 *
 * \section xsdcontrols Control devices
 *
 * Control devices provide the 'intelligence' to Torc's behaviour. They generally take one or more input values (from input
 * devices or other control devices) and generate a different output value. Control devices can output to one or more other control devices
 * , some notification devices and output devices.
 *
 * -# Timer devices
 *
 * Timer devices <b>must</b> have no inputs.
 *
 * -# Transition devices
 *
 * Transition devices <b>must</b> have exactly one input and <b>must</b> also specifiy a duration for the transition.
 *
 * -# Logic devices
 *
 *
 *
 * \section xsdlimits Configuration errors not (currently) identified by XSD validation.
 * -# Circular device references.
 * -# Self reference (i.e. a direct circular reference).
 * -# Multiple inputs to an output.
 * -# Reciprocal outputs/inputs (i.e. matched)
*/
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
    bool allfailed = true;
    foreach (QString file, testfiles)
    {
        // these should all FAIL!
        QString path = directory + "/" + file;
        TorcXmlValidator validator(path, fullxsd, true);
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

