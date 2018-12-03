#ifndef TORCLOGGING_H_
#define TORCLOGGING_H_

#ifdef __cplusplus
#include <QString>
#endif
#include <stdint.h>
#include <errno.h>

#include "torcloggingdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VERBOSE_LEVEL_NONE (gVerboseMask == 0)
#define VERBOSE_LEVEL_CHECK(_MASK_, _LEVEL_) \
    (((gVerboseMask & (_MASK_)) == (_MASK_)) && gLogLevel >= (_LEVEL_))

#ifdef __cplusplus
#define LOG(_MASK_, _LEVEL_, _STRING_)                                  \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            PrintLogLine(_MASK_, (LogLevel)_LEVEL_,                     \
                         __FILE__, __LINE__, __FUNCTION__, 1,           \
                         QString(_STRING_).toLocal8Bit().constData());  \
        }                                                               \
    } while (false)
#else
#define LOG(_MASK_, _LEVEL_, _FORMAT_, ...)                             \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            PrintLogLine(_MASK_, (LogLevel)_LEVEL_,                     \
                         __FILE__, __LINE__, __FUNCTION__, 0,           \
                         (const char *)_FORMAT_, ##__VA_ARGS__);        \
        }                                                               \
    } while (0)
#endif

void PrintLogLine(uint64_t mask, LogLevel level, const char *file, int line,
                  const char *function, int fromQString, const char *format, ...);

extern LogLevel gLogLevel;
extern uint64_t gVerboseMask;

#ifdef __cplusplus
}

extern QString    gLogPropagationArgs;
extern QString    gVerboseString;

void     StartLogging(const QString &Logfile, int progress = 0,
                      int quiet = 0, const QString &level = QStringLiteral("info"),
                                       bool Propagate = false);
void     StopLogging(void);
void     CalculateLogPropagation(void);
bool     GetQuietLogPropagation(void);
LogLevel GetLogLevel(const QString &level);
QString  GetLogLevelName(LogLevel level);
int      ParseVerboseArgument(const QString &arg);
QString  LogErrorToString(int errnum);

/// This can be appended to the LOG args with 
/// "+".  Please do not use "<<".  It uses
/// a thread safe version of strerror to produce the
/// string representation of errno and puts it on the
/// next line in the verbose output.
#define ENO (QString("\n\t\t\teno: ") + LogErrorToString(errno))
#define ENO_STR ENO.toLocal8Bit().constData()
#endif // __cplusplus

#endif

// vim:ts=4:sw=4:ai:et:si:sts=4
