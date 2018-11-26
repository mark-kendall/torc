#check QT major version
lessThan(QT_MAJOR_VERSION, 5) {
    error("Must build against Qt5")
}

# Environment overrides
# Force use of graphviz libraries. Currently disabled due to leaks.
libgvc = $$(TORC_LIBGVC)
# Force libxml2 check
libxml2 = $$(TORC_LIBXML2)
# Force Pi build due to incorrect Raspbian makespec
pi = $$(TORC_PI)
# OpenMax - disabled by default
openmax =

TEMPLATE    = app
CONFIG     += thread console
CONFIG     -= app_bundle
CONFIG     += c++11
TARGET      = torc-server
PREFIX      = /usr/local
RUNPREFIX   = $$PREFIX
target.path = $${PREFIX}/bin
INSTALLS    = target

DEFINES    += PREFIX=\\\"$${PREFIX}\\\"
DEFINES    += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES    += GIT_VERSION='"\\\"$(shell git describe --always)\\\""'
DEFINES    += _GNU_SOURCE
DEFINES    += __STDC_LIMIT_MACROS

QT         += sql network
QT         -= gui

QMAKE_CXXFLAGS += -Wall -Wextra -Weffc++ -Werror

# explicitly add SSL - this may need more work
macx {
    # NB this is a brew workaround
    LIBS        += -L/usr/local/opt/openssl/lib -lssl
    LIBS        += -L/usr/local/opt/openssl/lib -lcrypto
    INCLUDEPATH += /usr/local/opt/openssl/include
} else {
    CONFIG    += link_pkgconfig
    PKGCONFIG += openssl
}

# debug builds
#CONFIG     += debug
CONFIG(debug, debug|release) {
    message("Debug build")
}else {
    message("Release build")
}

# libraries
# zlib on windows is too much like hard work
win32 {
    message("Zlib support NOT available for compressed html")
}
else
{
    message("Zlib support available for compressed html")
    DEFINES += USING_ZLIB
    LIBS    += -lz
}

!mac:!win32:LIBS += -lrt

QMAKE_CXXFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

DEPENDPATH  += ./torc ./torc/http ./torc/upnp ./inputs ./inputs/platforms ./server
DEPENDPATH  += ./outputs ./outputs/platforms ./torc/openmax ./controls ./notify
INCLUDEPATH += $$DEPENDPATH

# use graphviz via library or executable?
!isEmpty(libgvc):packagesExist(libgvc) {
    DEFINES += USING_GRAPHVIZ_LIBS
    CONFIG  += link_pkgconfig
    PKGCONFIG += libgvc
    message("Linking to graphviz libraries for svg generation")
} else {
    message("Using external graphviz binary for svg generation (if available)")
}

# translations
translations.path    = $${PREFIX}/share/torc/i18n/
translations.files  += i18n/*.qm
INSTALLS            += translations

# HTML
install.path   = $${PREFIX}/share/torc/html/
install.files  = html/index.html
install.files += html/torc.xsd
install.files += html/manifest.json
install.files += html/browserconfig.xml
install.files += html/css html/fonts html/img html/js
INSTALLS      += install

# libxml2 for xml validation
!win32 {
    packagesExist(libxml-2.0) | !isEmpty(libxml2) {
        DEFINES += USING_LIBXML2
        HEADERS += torc/torclibxmlvalidator.h
        SOURCES += torc/torclibxmlvalidator.cpp
        if (isEmpty(libxml2)) {
            message("libxml2 available for xml validation")
            CONFIG += link_pkgconfig
            PKGCONFIG += libxml-2.0
        } else {
            message("libxml2 available for xml validation (forced)")
            LIBS        += -lxml2
            INCLUDEPATH += $${PREFIX}/include/libxml2
        }
    } else {
        message("No libxml2 for xml validation")
    }
}

# xmlpatterns module for xml validation
# xmlpatterns is slow and buggy - disable by default
qtHaveModule(xmlpatterns_XX) {
    QT += xmlpatterns
    DEFINES += USING_XMLPATTERNS
    HEADERS += torc/torcxmlvalidator.h
    SOURCES += torc/torcxmlvalidator.cpp
    message("QtXmlPatterns available")
}

linux {
    # linux power support
    qtHaveModule(dbus) {
        QT += dbus
        HEADERS += torc/platforms/torcpowerunixdbus.h
        SOURCES += torc/platforms/torcpowerunixdbus.cpp
        DEFINES += USING_QTDBUS
        message("QtDBus available for power support")
    } else {
        message("No QtDBus support for power support")
    }
    # OpenMax Bellagio (test) NB install components package as well for test components
    packagesExist(libomxil-bellagio) {
        openmax    = true
        DEFINES   += USING_LIBOMXIL_BELLAGIO
        CONFIG    += link_pkgconfig
        PKGCONFIG += libomxil-bellagio
        message("Linking to OpenMax Bellagio library")
    }
}

# OS X power support
macx {
    QMAKE_OBJECTIVE_CXXFLAGS += $$QMAKE_CXXFLAGS
    LIBS += -framework Cocoa -framework IOkit
    OBJECTIVE_HEADERS += torc/platforms/torccocoa.h
    OBJECTIVE_SOURCES += torc/platforms/torccocoa.mm
    HEADERS += torc/platforms/torcosxutils.h
    HEADERS += torc/platforms/torcpowerosx.h
    HEADERS += torc/platforms/torcrunlooposx.h
    SOURCES += torc/platforms/torcosxutils.cpp
    SOURCES += torc/platforms/torcpowerosx.cpp
    SOURCES += torc/platforms/torcrunlooposx.cpp
}

# Raspberry Pi build
# Qt5 distributed with Raspbian Jessie uses the generic linux-g++ makespec, not linux-rasp-pi-g++
# so force it with an environment variable if necessary (i.e. TORC_PI=1 qmake).
linux-rasp-pi-g++ | !isEmpty(pi) {
    LIBS += -lwiringPi
    DEFINES += USING_I2C
    DEFINES += USING_PIGPIO
    HEADERS += outputs/platforms/torci2cpca9685.h
    HEADERS += outputs/platforms/torcpigpio.h
    HEADERS += outputs/platforms/torcpiswitchoutput.h
    HEADERS += outputs/platforms/torcpipwmoutput.h
    HEADERS += inputs/platforms/torcpiswitchinput.h
    HEADERS += outputs/platforms/torcpicamera.h
    SOURCES += outputs/platforms/torci2cpca9685.cpp
    SOURCES += outputs/platforms/torcpigpio.cpp
    SOURCES += outputs/platforms/torcpiswitchoutput.cpp
    SOURCES += outputs/platforms/torcpipwmoutput.cpp
    SOURCES += inputs/platforms/torcpiswitchinput.cpp
    SOURCES += outputs/platforms/torcpicamera.cpp

    openmax      = true
    LIBS        += -L/opt/vc/lib -lbcm_host -lopenmaxil
    DEFINES     += USING_LIBOPENMAXIL
    INCLUDEPATH += /opt/vc/include
    INCLUDEPATH += /opt/vc/include/IL
    INCLUDEPATH += /opt/vc/include/interface/vcos/pthreads
    INCLUDEPATH += /opt/vc/include/interface/vmcs_host/linux
    message("Linking to OpenMaxIL library (Raspberry Pi)")

    DEFINES     += USING_FFMPEG
    CONFIG      += link_pkgconfig
    PKGCONFIG   += libavformat
    PKGCONFIG   += libavcodec
    PKGCONFIG   += libavutil
    HEADERS     += torc/ffmpeg/torcmuxer.h
    SOURCES     += torc/ffmpeg/torcmuxer.cpp
    INCLUDEPATH += ./torc/ffmpeg
    message("Linking to ffmpeg")

    # install with suid permissions on Pi
    # this allows access to I2C and GPIO
    setpriv.target       = setpriv
    setpriv.depends      = FORCE
    setpriv.commands     = @echo set privileges for $$target.path/$$TARGET && chmod u+s $$target.path/$$TARGET
    QMAKE_EXTRA_TARGETS += setpriv
    setpriv.path         = .setpriv
    INSTALLS            += setpriv

    message("Building for Raspberry Pi")
}

!isEmpty(openmax) {
    #HEADERS += torc/openmax/torcomxtest.h
    HEADERS += torc/openmax/torcomxcore.h
    HEADERS += torc/openmax/torcomxport.h
    HEADERS += torc/openmax/torcomxcomponent.h
    HEADERS += torc/openmax/torcomxtunnel.h
    #SOURCES += torc/openmax/torcomxtest.cpp
    SOURCES += torc/openmax/torcomxcore.cpp
    SOURCES += torc/openmax/torcomxport.cpp
    SOURCES += torc/openmax/torcomxcomponent.cpp
    SOURCES += torc/openmax/torcomxtunnel.cpp
}

# Bonjour is not available on windows
win32 {
    SOURCES += torc/platforms/torcbonjourwindows.cpp
    message("Bonjour NOT available for peer detection")
} else {
    linux {
        CONFIG    += link_pkgconfig
        PKGCONFIG += avahi-compat-libdns_sd
    }
    SOURCES += torc/torcbonjour.cpp
    message("Bonjour available for peer detection")
}

HEADERS += torc/torclogging.h
HEADERS += torc/torcloggingdefs.h
HEADERS += torc/torcqthread.h
HEADERS += torc/torcloggingimp.h
HEADERS += torc/torcplist.h
HEADERS += torc/torccompat.h
HEADERS += torc/torcexitcodes.h
HEADERS += torc/torctimer.h
HEADERS += torc/torclocalcontext.h
HEADERS += torc/torcnetworkedcontext.h
HEADERS += torc/torcsqlitedb.h
HEADERS += torc/torcdb.h
HEADERS += torc/torcmaths.h
HEADERS += torc/torcreferencecounted.h
HEADERS += torc/torccoreutils.h
HEADERS += torc/torccommandline.h
HEADERS += torc/torcevent.h
HEADERS += torc/torcobservable.h
HEADERS += torc/torclocaldefs.h
HEADERS += torc/torcpower.h
HEADERS += torc/torcadminthread.h
HEADERS += torc/torclanguage.h
HEADERS += torc/torcnetwork.h
HEADERS += torc/torcnetworkrequest.h
HEADERS += torc/torcsetting.h
HEADERS += torc/torcrpcrequest.h
HEADERS += torc/torcdirectories.h
HEADERS += torc/torcmime.h
HEADERS += torc/torcbonjour.h
HEADERS += torc/torcxmlreader.h
HEADERS += torc/torctime.h
HEADERS += torc/torcuser.h
HEADERS += torc/torcsegmentedringbuffer.h
HEADERS += torc/http/torchttprequest.h
HEADERS += torc/http/torchttpservice.h
HEADERS += torc/http/torchttpservices.h
HEADERS += torc/http/torchttpserver.h
HEADERS += torc/http/torchttpserverlistener.h
HEADERS += torc/http/torchttpservernonce.h
HEADERS += torc/http/torchtmlhandler.h
HEADERS += torc/http/torchtmlstaticcontent.h
HEADERS += torc/http/torchtmldynamiccontent.h
HEADERS += torc/http/torchttphandler.h
HEADERS += torc/http/torchttpreader.h
HEADERS += torc/http/torcwebsocket.h
HEADERS += torc/http/torcwebsocketreader.h
HEADERS += torc/http/torcwebsocketthread.h
HEADERS += torc/http/torcwebsocketpool.h
HEADERS += torc/http/torcwebsockettoken.h
HEADERS += torc/http/torcserialiser.h
HEADERS += torc/http/torcxmlserialiser.h
HEADERS += torc/http/torcjsonserialiser.h
HEADERS += torc/http/torcplistserialiser.h
HEADERS += torc/http/torcplaintextserialiser.h
HEADERS += torc/http/torcbinaryplistserialiser.h
HEADERS += torc/http/torcjsonrpc.h
HEADERS += torc/upnp/torcupnp.h
HEADERS += torc/upnp/torcssdp.h
HEADERS += torc/upnp/torcupnpcontent.h
HEADERS += inputs/torcinput.h
HEADERS += inputs/torcinputs.h
HEADERS += inputs/torcpwminput.h
HEADERS += inputs/torcswitchinput.h
HEADERS += inputs/torcphinput.h
HEADERS += inputs/torcnetworkswitchinput.h
HEADERS += inputs/torcnetworkpwminput.h
HEADERS += inputs/torcnetworktemperatureinput.h
HEADERS += inputs/torcnetworkphinput.h
HEADERS += inputs/torcnetworkbuttoninput.h
HEADERS += inputs/torcnetworkinputs.h
HEADERS += inputs/torctemperatureinput.h
HEADERS += inputs/torcsysteminputs.h
HEADERS += inputs/torcsysteminput.h
HEADERS += inputs/platforms/torc1wirebus.h
HEADERS += inputs/platforms/torc1wireds18b20.h
HEADERS += outputs/torcoutput.h
HEADERS += outputs/torcoutputs.h
HEADERS += outputs/torcpwmoutput.h
HEADERS += outputs/torcswitchoutput.h
HEADERS += outputs/torcphoutput.h
HEADERS += outputs/torctemperatureoutput.h
HEADERS += outputs/platforms/torci2cbus.h
HEADERS += outputs/torcnetworkpwmoutput.h
HEADERS += outputs/torcnetworkswitchoutput.h
HEADERS += outputs/torcnetworktemperatureoutput.h
HEADERS += outputs/torcnetworkphoutput.h
HEADERS += outputs/torcnetworkbuttonoutput.h
HEADERS += outputs/torccamera.h
HEADERS += outputs/torccamerathread.h
HEADERS += outputs/torccameraoutput.h
HEADERS += controls/torccontrol.h
HEADERS += controls/torccontrols.h
HEADERS += controls/torclogiccontrol.h
HEADERS += controls/torctimercontrol.h
HEADERS += controls/torctransitioncontrol.h
HEADERS += notify/torcnotify.h
HEADERS += notify/torcnotifier.h
HEADERS += notify/torclognotifier.h
HEADERS += notify/torcpushbulletnotifier.h
HEADERS += notify/torciotlogger.h
HEADERS += notify/torcthingspeaknotifier.h
HEADERS += notify/torciotplotternotifier.h
HEADERS += notify/torcnotification.h
HEADERS += notify/torcsystemnotification.h
HEADERS += notify/torctriggernotification.h

SOURCES += torc/torcloggingimp.cpp
SOURCES += torc/torcplist.cpp
SOURCES += torc/torcqthread.cpp
SOURCES += torc/torclocalcontext.cpp
SOURCES += torc/torcnetworkedcontext.cpp
SOURCES += torc/torctimer.cpp
SOURCES += torc/torcsqlitedb.cpp
SOURCES += torc/torcdb.cpp
SOURCES += torc/torccoreutils.cpp
SOURCES += torc/torcreferencecounted.cpp
SOURCES += torc/torccommandline.cpp
SOURCES += torc/torcevent.cpp
SOURCES += torc/torcobservable.cpp
SOURCES += torc/torcpower.cpp
SOURCES += torc/torcadminthread.cpp
SOURCES += torc/torclanguage.cpp
SOURCES += torc/torcnetwork.cpp
SOURCES += torc/torcnetworkrequest.cpp
SOURCES += torc/torcsetting.cpp
SOURCES += torc/torcrpcrequest.cpp
SOURCES += torc/torcdirectories.cpp
SOURCES += torc/torcmime.cpp
SOURCES += torc/torcxmlreader.cpp
SOURCES += torc/torctime.cpp
SOURCES += torc/torcuser.cpp
SOURCES += torc/torcsegmentedringbuffer.cpp
SOURCES += torc/http/torchttprequest.cpp
SOURCES += torc/http/torchttpserver.cpp
SOURCES += torc/http/torchttpserverlistener.cpp
SOURCES += torc/http/torchttpservernonce.cpp
SOURCES += torc/http/torchtmlhandler.cpp
SOURCES += torc/http/torchtmlstaticcontent.cpp
SOURCES += torc/http/torchtmldynamiccontent.cpp
SOURCES += torc/http/torchttphandler.cpp
SOURCES += torc/http/torchttpreader.cpp
SOURCES += torc/http/torchttpservice.cpp
SOURCES += torc/http/torchttpservices.cpp
SOURCES += torc/http/torcwebsocket.cpp
SOURCES += torc/http/torcwebsocketreader.cpp
SOURCES += torc/http/torcwebsocketthread.cpp
SOURCES += torc/http/torcwebsocketpool.cpp
SOURCES += torc/http/torcwebsockettoken.cpp
SOURCES += torc/http/torcserialiser.cpp
SOURCES += torc/http/torcxmlserialiser.cpp
SOURCES += torc/http/torcjsonserialiser.cpp
SOURCES += torc/http/torcplistserialiser.cpp
SOURCES += torc/http/torcplaintextserialiser.cpp
SOURCES += torc/http/torcbinaryplistserialiser.cpp
SOURCES += torc/http/torcjsonrpc.cpp
SOURCES += torc/upnp/torcupnp.cpp
SOURCES += torc/upnp/torcssdp.cpp
SOURCES += torc/upnp/torcupnpcontent.cpp
SOURCES += inputs/torcinput.cpp
SOURCES += inputs/torcinputs.cpp
SOURCES += inputs/torcpwminput.cpp
SOURCES += inputs/torcphinput.cpp
SOURCES += inputs/torcswitchinput.cpp
SOURCES += inputs/torctemperatureinput.cpp
SOURCES += inputs/torcnetworkswitchinput.cpp
SOURCES += inputs/torcnetworkpwminput.cpp
SOURCES += inputs/torcnetworktemperatureinput.cpp
SOURCES += inputs/torcnetworkphinput.cpp
SOURCES += inputs/torcnetworkbuttoninput.cpp
SOURCES += inputs/torcnetworkinputs.cpp
SOURCES += inputs/torcsysteminputs.cpp
SOURCES += inputs/torcsysteminput.cpp
SOURCES += inputs/platforms/torc1wirebus.cpp
SOURCES += inputs/platforms/torc1wireds18b20.cpp
SOURCES += outputs/torcoutput.cpp
SOURCES += outputs/torcoutputs.cpp
SOURCES += outputs/torcpwmoutput.cpp
SOURCES += outputs/torcswitchoutput.cpp
SOURCES += outputs/torctemperatureoutput.cpp
SOURCES += outputs/torcphoutput.cpp
SOURCES += outputs/platforms/torci2cbus.cpp
SOURCES += outputs/torcnetworkpwmoutput.cpp
SOURCES += outputs/torcnetworkswitchoutput.cpp
SOURCES += outputs/torcnetworktemperatureoutput.cpp
SOURCES += outputs/torcnetworkphoutput.cpp
SOURCES += outputs/torcnetworkbuttonoutput.cpp
SOURCES += outputs/torccamera.cpp
SOURCES += outputs/torccamerathread.cpp
SOURCES += outputs/torccameraoutput.cpp
SOURCES += controls/torccontrol.cpp
SOURCES += controls/torccontrols.cpp
SOURCES += controls/torclogiccontrol.cpp
SOURCES += controls/torctimercontrol.cpp
SOURCES += controls/torctransitioncontrol.cpp
SOURCES += notify/torcnotify.cpp
SOURCES += notify/torcnotifier.cpp
SOURCES += notify/torclognotifier.cpp
SOURCES += notify/torciotlogger.cpp
SOURCES += notify/torcpushbulletnotifier.cpp
SOURCES += notify/torciotplotternotifier.cpp
SOURCES += notify/torcthingspeaknotifier.cpp
SOURCES += notify/torcnotification.cpp
SOURCES += notify/torcsystemnotification.cpp
SOURCES += notify/torctriggernotification.cpp

HEADERS += server/torccentral.h
HEADERS += server/torcdevice.h
HEADERS += server/torcdevicehandler.h
HEADERS += server/torcxsdtest.h
SOURCES += server/main.cpp
SOURCES += server/torccentral.cpp
SOURCES += server/torcdevice.cpp
SOURCES += server/torcdevicehandler.cpp
SOURCES += server/torcxsdtest.cpp

test {
    message("Building tests")
    DEFINES += TORC_TEST
    QMAKE_CXXFLAGS -= -O2
    QMAKE_CXXFLAGS += --coverage
    LIBS += --coverage
    QT += testlib
    TARGET = torc-tests
    target.path = ./
    INSTALLS = target
    SOURCES -= server/main.cpp
    SOURCES += test/main.cpp
    HEADERS += test/testserialisers.h
    HEADERS += test/testtorclocalcontext.h
    SOURCES += test/testserialisers.cpp
    SOURCES += test/testtorclocalcontext.cpp
}

QMAKE_CLEAN += $(TARGET)
