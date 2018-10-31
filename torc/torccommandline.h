#ifndef TORCCOMMANDLINE_H
#define TORCCOMMANDLINE_H

// Qt
#include <QObject>
#include <QVariant>
#include <QHash>

// Torc
#include "torclogging.h"

class TorcArgument;

class TorcCommandLine
{
    Q_GADGET
    Q_FLAGS(Options)

  public:
    enum Option
    {
        None     = (0 << 0),
        Help     = (1 << 0),
        Version  = (1 << 1),
        LogLevel = (1 << 2),
        LogType  = (1 << 3),
        Database = (1 << 4),
        LogFile  = (1 << 5),
        XSDTest  = (1 << 6),
        ConfDir  = (1 << 7)
    };

    Q_DECLARE_FLAGS(Options, Option)

  public:
    explicit TorcCommandLine(TorcCommandLine::Options Flags);
    ~TorcCommandLine();

    static bool   RegisterEnvironmentVariable  (const QString &Var, const QString &Description);

    int           Evaluate                     (int argc, const char * const * argv, bool &Exit);
    void          Add                          (const QString Keys, const QVariant &Default, const QString &HelpText, bool ExitImmediately/*=false*/);
    QVariant      GetValue                     (const QString &Key);

  private:
    void          AddPriv                      (const QString Keys, const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags = TorcCommandLine::None, bool ExitImmediately = false);

  private:
    QHash<QString,TorcArgument> m_options;
    QHash<QString,QString>      m_aliases;
    QHash<QString,QString>      m_help;
    uint                        m_maxLength;
};

class TorcArgument
{
  public:
    TorcArgument();
    TorcArgument(const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags, bool Exit);

    QVariant                 m_value;
    QString                  m_helpText;
    bool                     m_exitImmediately;
    TorcCommandLine::Options m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TorcCommandLine::Options)

#endif // TORCCOMMANDLINE_H
