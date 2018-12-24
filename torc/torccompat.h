#ifndef TORCCOMPAT_H
#define TORCCOMPAT_H

#ifdef _MSC_VER
#undef USING_MINGW
#include <time.h>
#endif

#ifdef _WIN32
# ifndef _MSC_VER
#  define close wsock_close
# endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _MSC_VER
# include <winsock2.h>
# include <ws2tcpip.h>
#else
#  include <io.h>
# endif

#include <windows.h>
# undef DialogBox
# undef LoadImage
# undef LoadIcon
# undef GetObject
# undef DrawText
# undef CreateDialog
# undef CreateFont
# undef DeleteFile
# undef GetCurrentTime
# undef SetJob
# undef SendMessage

# define setsockopt(a, b, c, d, e) setsockopt(a, b, c, (const char*)(d), e)
# undef close
# include <stdio.h>        // for snprintf(), used by inline dlerror()
//# include <unistd.h>       // for usleep()
#else
# include <sys/time.h>     // Mac OS X needs this before sys/resource
# include <sys/resource.h> // for setpriority
# include <sys/socket.h>
# include <sys/wait.h>     // For WIFEXITED on Mac OS X
#endif

#ifdef _MSC_VER
    // Turn off the visual studio warnings (identifier was truncated)
    #pragma warning(disable:4786)

    #ifdef restrict 
    #undef restrict
    #endif

    typedef int pid_t;
    typedef int tid;
    #include <stdint.h>
    #include <direct.h>
    #include <process.h>

    #define strtoll             _strtoi64
    #define strncasecmp         _strnicmp
    #define snprintf            _snprintf

    #ifdef  _WIN64
        typedef __int64    ssize_t;
    #else
        typedef int   ssize_t;
    #endif

    // Check for execute, only checking existance in MSVC
    #define X_OK    0

    #define rint( x )               floor(x + 0.5)
    #define round( x )              floor(x + 0.5)
    #define getpid()                _getpid()
    #define ftruncate( fd, fsize )  _chsize( fd, fsize ) 

    #ifndef S_ISCHR
    #   ifdef S_IFCHR
    #       define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
    #   else
    #       define S_ISCHR(m) 0
    #   endif
    #endif /* !S_ISCHR */

    #ifndef S_ISBLK
    #   define S_ISBLK(m) 0
    #endif 

    #ifndef S_ISREG
    #   define S_ISREG(m) 1
    #endif 

    #ifndef S_ISDIR
    #  ifdef S_IFDIR
    #       define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR )
    #   else
    #       define S_ISDIR(m) 0
    #   endif
    #endif 
#endif

#ifdef _WIN32
typedef unsigned int uint;
# undef M_PI
# define M_PI 3.14159265358979323846
#endif

#ifdef USING_MINGW
#include <unistd.h>       // for usleep()
#include <stdlib.h>       // for rand()
#include <time.h>
#include <sys/time.h>
#define SIGHUP  1
#define SIGQUIT 3
#define SIGKILL 9
#define SIGUSR1 10  // used to force UPnP mediamap rebuild in the backend
#define SIGUSR2 12  // used to restart LIRC as required
#define SIGPIPE 13  // not implemented in MINGW, will produce "unable to ignore sigpipe"
#define SIGALRM 14
#define SIGCONT 18
#define SIGSTOP 19
#define O_SYNC 0
#define getuid() 0
#define geteuid() 0
#define setuid(x)
#endif // USING_MINGW

#if defined(_MSC_VER) && !defined(localtime_r)
static inline struct tm* localtime_r(const time_t *timep, struct tm *result)
{
	localtime_s(result, timep);
	return result;
}
#endif

#ifndef _MSC_VER
#include <sys/param.h>  // Defines BSD on FreeBSD, Mac OS X
#endif
#include <sys/stat.h>   // S_IREAD/WRITE on MinGW, umask() on BSD

// suseconds_t
#include <sys/types.h>

#if defined(Q_OS_MAC)
#include <sys/types.h>
#include <unistd.h>
typedef off_t off64_t;
#endif

#if defined(_MSC_VER)
#  define S_IRUSR _S_IREAD
#  ifndef lseek64
#    define lseek64( f, o, w ) _lseeki64( f, o, w )
#  endif
#endif

#ifdef _WIN32
#define PREFIX64 "I64"
#else
#define PREFIX64 "ll"
#endif

#endif // TORCCOMPAT_H
