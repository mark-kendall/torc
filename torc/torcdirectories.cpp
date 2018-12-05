/* TorcDirectories
*
* This file is part of the Torc project.
*
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QDir>

// Torc
#include "torclogging.h"
#include "torclocaldefs.h"
#include "torcdirectories.h"

static QString gInstallDir = QStringLiteral("");
static QString gShareDir   = QStringLiteral("");
static QString gConfDir    = QStringLiteral("");
static QString gTransDir   = QStringLiteral("");
static QString gContentDir = QStringLiteral("");

/*! \brief Statically initialise the various directories that Torc uses.
 *
 * gInstallDir will default to /usr/local
 * gShareDir will default to  /usr/local/share/
 * gTransDir will default to  /usr/local/share/torc/i18n/
 * gConfDir will default to ~/.torc
 * gContentDir will default to ~/.torc/content/
 *
 * \sa GetTorcConfigDir
 * \sa GetTorcShareDir
 * \sa GetTorcTransDir
 * \sa GetTorcContentDir
*/
void InitialiseTorcDirectories(TorcCommandLine* CommandLine)
{
    static bool initialised = false;
    if (initialised)
        return;

    initialised = true;

    gInstallDir = QStringLiteral("%1/").arg(QStringLiteral(PREFIX));
    gShareDir   = QStringLiteral("%1/share/%2").arg(QStringLiteral(RUNPREFIX), TORC_TORC);
    // override shared directory
    QString sharedir = CommandLine ? CommandLine->GetValue(QStringLiteral("share")).toString() : QStringLiteral();
    if (!sharedir.isEmpty())
        gShareDir = sharedir;

    gTransDir   = gShareDir + QStringLiteral("/i18n/");
    // override translation directory - which may be done independantly of shared directory
    QString transdir = CommandLine ? CommandLine->GetValue(QStringLiteral("trans")).toString() : QStringLiteral();
    if (!transdir.isEmpty())
        gTransDir = transdir;

    gConfDir    = QDir::homePath() + "/." + TORC_TORC;
    // override config directory - and by implication the content directory
    QString confdir = CommandLine ? CommandLine->GetValue(QStringLiteral("config")).toString() : QStringLiteral();
    if (!confdir.isEmpty())
        gConfDir = confdir;
    gContentDir = gConfDir + TORC_CONTENT_DIR;
}

/*! \brief Return the path to the application configuration directory
 *
 * This is the directory where (under a default setup) the database is stored.
*/
QString GetTorcConfigDir(void)
{
    return gConfDir;
}

/*! \brief Return the path to the installed Torc shared resources.
 *
 * Shared resources might include UI themes, other images and static HTML content.
*/
QString GetTorcShareDir(void)
{
    return gShareDir;
}

///brief Return the path to installed translation files
QString GetTorcTransDir(void)
{
    return gTransDir;
}

///brief Return the path to generated content
QString GetTorcContentDir(void)
{
    return gContentDir;
}
