#ifndef TORCLOCALDEFS_H
#define TORCLOCALDEFS_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0502 // XP SP 2
#endif

#define TORC_TORC         QString("torc") // N.B. do NOT change
#define TORC_REALM        QString("Torc") // N.B. do NOT change
#define TORC_MAIN_THREAD  QString("MainLoop")
#define TORC_ADMIN_THREAD QString("AdminLoop")
#define TORC_SERVICES_DIR QString("/services/")
#define TORC_USER_SERVICE QString("user")
#define TORC_PORT_SERVICE QString("ServerPort")
#define TORC_SSL_SERVICE  QString("ServerSecure")
#define TORC_SETTINGS_DIR QString("settings/")
#define TORC_ROOT_SETTING QString("Settings")
#define TORC_CONFIG_FILE  (TORC_TORC + QString(".xml"))
#define TORC_CONTENT_DIR  QString("/content/")

#endif // TORCLOCALDEFS_H
