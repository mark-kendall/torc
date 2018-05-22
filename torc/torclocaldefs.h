#ifndef TORCLOCALDEFS_H
#define TORCLOCALDEFS_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0502 // XP SP 2
#endif

#define TORC_MAIN_THREAD  QString("MainLoop")
#define TORC_ADMIN_THREAD QString("AdminLoop")
#define TORC_ROOT_SETTING QString("Top")
#define TORC_SERVICES_DIR QString("/services/")
#define TORC_USER_SERVICE QString("user")
#define TORC_REALM        QString("Torc")       // N.B. do NOT change

#endif // TORCLOCALDEFS_H
