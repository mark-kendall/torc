#ifndef TORCLOCALDEFS_H
#define TORCLOCALDEFS_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0502 // XP SP 2
#endif

#define TORC_TORC         QStringLiteral("torc") // N.B. do NOT change
#define TORC_REALM        QStringLiteral("Torc") // N.B. do NOT change
#define TORC_MAIN_THREAD  QStringLiteral("MainLoop")
#define TORC_ADMIN_THREAD QStringLiteral("AdminLoop")
#define TORC_SERVICES_DIR QStringLiteral("/services/")
#define TORC_USER_SERVICE QStringLiteral("user")
#define TORC_PORT_SERVICE QStringLiteral("ServerPort")
#define TORC_SSL_SERVICE  QStringLiteral("ServerSecure")
#define TORC_SETTINGS_DIR QStringLiteral("settings/")
#define TORC_ROOT_SETTING QStringLiteral("Settings")
#define TORC_CONFIG_FILE  (TORC_TORC + QStringLiteral(".xml"))
#define TORC_CONTENT_DIR  QStringLiteral("/content/")

#endif // TORCLOCALDEFS_H
