/* Class TorcCommandLine
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013-18
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
#include <QMutex>
#include <QStringList>
#include <QCoreApplication>

// Torc
#include "torcexitcodes.h"
#include "torclogging.h"
#include "torccommandline.h"

#include <iostream>

/*! \class TorcEnvironmentVariable
 *  \brief A simple private linked list to manage registered environment variables.
*/
class TorcEnvironmentVariable
{
  public:
    TorcEnvironmentVariable(const QString &Var, const QString &Description)
      : nextEnvironmentVariable(gEnvironmentVariable),
        m_variable(Var),
        m_description(Description)
    {
        gEnvironmentVariable = this;
    }

    static TorcEnvironmentVariable* gEnvironmentVariable;
    TorcEnvironmentVariable*        nextEnvironmentVariable;
    QString                         m_variable;
    QString                         m_description;

  private:
    Q_DISABLE_COPY(TorcEnvironmentVariable)
};

TorcEnvironmentVariable* TorcEnvironmentVariable::gEnvironmentVariable = nullptr;

/*! \class TorcArgument
 *  \brief Simple wrapper around a command line argument.
 *
 * The QVariant described by m_value indicates the type accepted by the argument. An
 * invalid QVariant indicates that the argument is an option only (e.g. --help) and requires
 * no value. When a value is detected in the command line, m_value is updated accordingly for
 * later retrieval, or is set to true for options to indicate the option was detected.
*/
TorcArgument::TorcArgument()
  : m_value(QVariant()),
    m_helpText(),
    m_exitImmediately(true),
    m_flags(TorcCommandLine::None)
{
}

TorcArgument::TorcArgument(const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags, bool Exit)
  : m_value(Default),
    m_helpText(HelpText),
    m_exitImmediately(Exit),
    m_flags(Flags)
{
}

/*! \class TorcCommandLine
 *  \brief Torc command line handler.
 *
 * TorcCommandLine will always add handling for help, log, verbose and version handling.
 *
 * Additional pre-defined handlers can be implemented by passing additional TorcCommandLine::Options flags to
 * the constructor.
 *
 * Custom command line options can be implemented by calling Add and retrieving the expected value via GetValue.
*/
TorcCommandLine::TorcCommandLine(Options Flags)
  : m_options(),
    m_aliases(),
    m_help(),
    m_maxLength(0)
{
    // always enable version, help and logging
    TorcCommandLine::Options options = Flags | TorcCommandLine::Version | TorcCommandLine::Help | TorcCommandLine::LogLevel |
                                               TorcCommandLine::LogType | TorcCommandLine::ConfDir | TorcCommandLine::ShareDir |
                                               TorcCommandLine::TransDir;

    if (options.testFlag(TorcCommandLine::Help))
        AddPriv(QStringLiteral("h,help"), QVariant(), QStringLiteral("Display full usage information."), TorcCommandLine::Help, true);
    if (options.testFlag(TorcCommandLine::LogLevel))
        AddPriv(QStringLiteral("l,log"), QStringList(QStringLiteral("general")), QStringLiteral("Set the logging level."), TorcCommandLine::None);
    if (options.testFlag(TorcCommandLine::LogType))
        AddPriv(QStringLiteral("v,verbose,verbosity"), QStringLiteral("info"), QStringLiteral("Set the logging type."), TorcCommandLine::None);
    if (options.testFlag(TorcCommandLine::Version))
        AddPriv(QStringLiteral("version"), QVariant(), QStringLiteral("Display version information."), TorcCommandLine::Version, true);
    if (options.testFlag(TorcCommandLine::Database))
        AddPriv(QStringLiteral("db,database"), QStringLiteral(""), QStringLiteral("Use a custom database location. If the file does not exist, it will be created."));
    if (options.testFlag(TorcCommandLine::LogFile))
        AddPriv(QStringLiteral("logfile"), QStringLiteral(""), QStringLiteral("Override the logfile location."));
    if (options.testFlag(TorcCommandLine::XSDTest))
        AddPriv(QStringLiteral("xsdtest"), QStringLiteral(""), QStringLiteral("Run validation of test configuration XML files found in the given directory."), TorcCommandLine::XSDTest);
    if (options.testFlag(TorcCommandLine::ConfDir))
        AddPriv(QStringLiteral("c,config"), QStringLiteral(""), QStringLiteral("Override the configuration directory for XML config file, database etc."));
    if (options.testFlag(TorcCommandLine::ShareDir))
        AddPriv(QStringLiteral("s,share"), QStringLiteral(""), QStringLiteral("Overrride the shared directory for HTML files etc"));
    if (options.testFlag(TorcCommandLine::TransDir))
        AddPriv(QStringLiteral("t,trans"), QStringLiteral(""), QStringLiteral("Override the translations directory"));
}

/*! \brief Implement custom command line options.
 *
 * \param Keys            A comma separated list of synonymous command line options (e.g. "h,help").
 * \param Default         The default value AND type for an option (e.g. QString("info") or (bool)true).
 * \param HelpText        Brief help text for the option.
 * \param ExitImmediately Tell the application to exit immediately after processing the command line.
*/
void TorcCommandLine::Add(const QString &Keys, const QVariant &Default, const QString &HelpText, bool ExitImmediately/*=false*/)
{
    AddPriv(Keys, Default, HelpText, TorcCommandLine::None, ExitImmediately);
}

void TorcCommandLine::AddPriv(const QString &Keys, const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags /*= TorcCommandLine::None*/, bool ExitImmediately /*=false*/)
{
    QStringList keys = Keys.split(',');

    QStringList valid;

    bool first = true;
    QString master;
    foreach (const QString &key, keys)
    {
        if (key.contains('='))
        {
            std::cout << QStringLiteral("Invalid option '%1'").arg(key).toLocal8Bit().constData() << std::endl;
        }
        else if (first && !m_options.contains(key))
        {
            master = key;
            m_options.insert(key, TorcArgument(Default, HelpText, Flags, ExitImmediately));
            valid.append("-" + key);
            first = false;
        }
        else if (!first && !m_aliases.contains(key) && !m_options.contains(key))
        {
            m_aliases.insert(key, master);
            valid.append("-" + key);
        }
        else
        {
            std::cout << QStringLiteral("Command line option '%1' already in use - ignoring").arg(key).toLocal8Bit().constData() << std::endl;
        }
    }

    if (!valid.isEmpty())
    {
        QString options = valid.join(QStringLiteral(" OR "));
        m_help.insert(options, HelpText);
        if (options.size() > (int)m_maxLength)
            m_maxLength = options.size() + 2;
    }
}

/*! \brief Evaluate the command line options.
 *
 * \param argc As passed to the main application at startup.
 * \param argv As passed to the main application at startup.
 * \param Exit Will be set to true if the application should exit after command line processing.
*/
int TorcCommandLine::Evaluate(int argc, const char * const *argv, bool &Exit)
{
    QString error;
    bool parserror    = false;
    int  result       = TORC_EXIT_OK;
    bool printhelp    = false;
    bool printversion = false;

    // loop through the command line arguments
    for (int i = 1; i < argc; ++i)
    {
        QString key = QString::fromLocal8Bit(argv[i]);

        // remove trailing '-'s
        while (key.startsWith('-'))
            key = key.mid(1);

        QString value(QStringLiteral(""));

        bool simpleformat = key.contains('=');
        if (simpleformat)
        {
            // option is --key=value format
            QStringList keyval = key.split('=');
            key = keyval.at(0).trimmed();
            value = keyval.at(1).trimmed();
        }

        // do we recognise this option
        if (!m_options.contains(key))
        {
            // is it an alias?
            if (!m_aliases.contains(key))
            {
                parserror = true;
                error = QStringLiteral("Unknown command line option '%1'").arg(key);
                break;
            }
            else
            {
                key = m_aliases.value(key);
            }
        }

        TorcArgument argument = m_options.value(key);

        // --key value format
        if (!simpleformat && argument.m_value.isValid())
        {
            if (i >= argc - 1)
            {
                parserror = true;
                error = QStringLiteral("Insufficient arguments - option '%1' requires a value").arg(key);
                break;
            }

            value = QString::fromLocal8Bit(argv[++i]).trimmed();

            if (value.startsWith('='))
            {
                parserror = true;
                error = QStringLiteral("Option '%1' expects a value").arg(key);
                break;
            }
        }

        if (!argument.m_value.isValid())
        {
            if (!value.isEmpty())
            {
                // unexpected value
                parserror = true;
                error = QStringLiteral("Option '%1' does not expect a value ('%2')").arg(key, value);
                break;
            }
            else
            {
                // mark option as detected
                argument.m_value = QVariant((bool)true);
            }
        }
        else if (argument.m_value.isValid() && value.isEmpty())
        {
            parserror = true;
            error = QStringLiteral("Option '%1' expects a value").arg(key).toLocal8Bit();
            break;
        }

        Exit         |= argument.m_exitImmediately;
        printhelp    |= (bool)(argument.m_flags & TorcCommandLine::Help);
        printversion |= (bool)(argument.m_flags & TorcCommandLine::Version);

        // update the value for the option
        if (!value.isEmpty())
        {
            switch ((QMetaType::Type)argument.m_value.type())
            {
                case QMetaType::QStringList:
                    argument.m_value = QVariant(QStringList(value));
                    break;
                default:
                    argument.m_value = QVariant(value);
            }
            // replace option with updated argument
            m_options.insert(key, argument);
        }
    }

    if (parserror)
    {
        result = TORC_EXIT_INVALID_CMDLINE;
        Exit = true;
        printhelp = true;
        std::cout << error.toLocal8Bit().constData() << std::endl << std::endl;
    }

    if (printhelp)
    {
        std::cout << "Command line options:" << std::endl << std::endl;

        QHash<QString,QString>::const_iterator it = m_help.constBegin();
        for ( ; it != m_help.constEnd(); ++it)
        {
            QByteArray option(it.key().toLocal8Bit().constData());
            QByteArray padding(m_maxLength - option.size(), 32);
            std::cout << option.constData() << padding.constData() << it.value().toLocal8Bit().constData() << std::endl;
        }

        std::cout << std::endl << "All options may be preceeded by '-' or '--'" << std::endl;

        {
            TorcEnvironmentVariable* variable = TorcEnvironmentVariable::gEnvironmentVariable;
            if (variable)
            {
                std::cout << std::endl << "The following environment variables may be useful:" << std::endl;

                while (variable)
                {
                    QByteArray var(variable->m_variable.toLocal8Bit().constData());
                    QByteArray padding(m_maxLength - var.size(), 32);
                    std::cout << var.constData() << padding.constData() << variable->m_description.toLocal8Bit().constData() << std::endl;
                    variable = variable->nextEnvironmentVariable;
                }
            }
        }
    }

    if (printversion)
    {
        std::cout << "QT Version : "   << QT_VERSION_STR << std::endl;
    }

    return result;
}

/*! \brief Return the value associated with Key or an invalid QVariant if the option is not present.
 *
 * \note In the case of options that require no value (e.g. --help), QVariant((bool)true) is returned if
 *       the option was detected.
*/
QVariant TorcCommandLine::GetValue(const QString &Key)
{
    if (m_options.contains(Key))
        return m_options.value(Key).m_value;

    if (m_aliases.contains(Key))
        return m_options.value(m_aliases.value(Key)).m_value;

    return QVariant();
}

///\brief Register an environment variable for display via the help option.
bool TorcCommandLine::RegisterEnvironmentVariable(const QString &Var, const QString &Description)
{
    return (bool)new TorcEnvironmentVariable(Var, Description);
}
