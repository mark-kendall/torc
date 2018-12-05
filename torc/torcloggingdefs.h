#ifndef TORCLOGGINGDEFS_H_
#define TORCLOGGINGDEFS_H_

#undef VERBOSE_PREAMBLE
#undef VERBOSE_POSTAMBLE
#undef VERBOSE_MAP

#undef LOGLEVEL_PREAMBLE
#undef LOGLEVEL_POSTAMBLE
#undef LOGLEVEL_MAP

#ifdef _IMPLEMENT_VERBOSE

// This is used to actually implement the mask in mythlogging.cpp
#define VERBOSE_PREAMBLE
#define VERBOSE_POSTAMBLE
#define VERBOSE_MAP(name,mask,additive,help) \
    AddVerbose((uint64_t)(mask),QStringLiteral(#name),(bool)(additive),QStringLiteral(help));

#define LOGLEVEL_PREAMBLE
#define LOGLEVEL_POSTAMBLE
#define LOGLEVEL_MAP(name,value,shortname) \
    AddLogLevel((int)(value),QStringLiteral(#name),(char)(shortname));

#else // !defined(_IMPLEMENT_VERBOSE)

// This is used to define the enumerated type (used by all files)
#define VERBOSE_PREAMBLE \
    enum VerboseMask {
#define VERBOSE_POSTAMBLE \
        VB_LAST_ITEM \
    };
#define VERBOSE_MAP(name,mask,additive,help) \
    name = (uint64_t)(mask),

#define LOGLEVEL_PREAMBLE \
    typedef enum {
#define LOGLEVEL_POSTAMBLE \
    } LogLevel;
#define LOGLEVEL_MAP(name,value,shortname) \
    name = (int)(value),

#endif

VERBOSE_PREAMBLE
VERBOSE_MAP(VB_ALL,       ~0ULL, false,
            "ALL available debug output")
VERBOSE_MAP(VB_GENERAL,   0x00000001, true,
            "General info")
VERBOSE_MAP(VB_NETWORK,   0x00000002, true,
            "Network protocol related messages")
VERBOSE_MAP(VB_UPNP,      0x00000004, true,
            "UPnP debugging messages")
VERBOSE_MAP(VB_FLUSH,     0x00000008, true,
            "")
/* Please do not add a description for VB_FLUSH, it
   should not be passed in via "-v". It is used to
   flush output to the standard output from console
   programs that have debugging enabled.
 */
VERBOSE_MAP(VB_STDIO,     0x00000010, true,
            "")
/* Please do not add a description for VB_STDIO, it
   should not be passed in via "-v". It is used to
   send output to the standard output from console
   programs that have debugging enabled.
 */
VERBOSE_MAP(VB_NONE,      0x00000000, false,
            "NO debug output")
VERBOSE_POSTAMBLE

LOGLEVEL_PREAMBLE
LOGLEVEL_MAP(LOG_ANY,    -1, ' ')
LOGLEVEL_MAP(LOG_EMERG,   0, '!')
LOGLEVEL_MAP(LOG_ALERT,   1, 'A')
LOGLEVEL_MAP(LOG_CRIT,    2, 'C')
LOGLEVEL_MAP(LOG_ERR,     3, 'E')
LOGLEVEL_MAP(LOG_WARNING, 4, 'W')
LOGLEVEL_MAP(LOG_NOTICE,  5, 'N')
LOGLEVEL_MAP(LOG_INFO,    6, 'I')
LOGLEVEL_MAP(LOG_DEBUG,   7, 'D')
LOGLEVEL_MAP(LOG_UNKNOWN, 8, '-')
LOGLEVEL_POSTAMBLE

#endif
