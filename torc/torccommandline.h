#ifndef TORCCOMMANDLINE_H
#define TORCCOMMANDLINE_H

// Qt
#include <QObject>

// Torc
#include "torclogging.h"

class TorcCommandLinePriv;

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
        XSDTest  = (1 << 6)
    };

    Q_DECLARE_FLAGS(Options, Option);

  public:
    TorcCommandLine(TorcCommandLine::Options Flags);
    ~TorcCommandLine();

    static bool   RegisterEnvironmentVariable  (const QString &Var, const QString &Description);

    int           Evaluate                     (int argc, const char * const * argv, bool &Exit);
    void          Add                          (const QString Keys, const QVariant &Default, const QString &HelpText, bool ExitImmediately = false);
    QVariant      GetValue                     (const QString &Key);

  private:
    TorcCommandLinePriv *m_priv;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TorcCommandLine::Options)

#endif // TORCCOMMANDLINE_H
