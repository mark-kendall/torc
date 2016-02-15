#check QT major version
lessThan(QT_MAJOR_VERSION, 5) {
    error("Must build against Qt5")
}

TEMPLATE    = app
CONFIG     += thread console
CONFIG     -= app_bundle
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

# debug builds
#CONFIG     += debug

# libraries
# zlib on windows is too much like hard work
win32 {
    message("Zlib support NOT available")
}
else
{
    message("Zlib support available")
    DEFINES += USING_ZLIB
    LIBS    += -lz
}

!mac:!win32:LIBS += -ldns_sd
!mac:!win32:LIBS += -lrt

DEPENDPATH  += ./torc ./torc/http ./torc/upnp ./inputs ./inputs/platforms
DEPENDPATH  += ./outputs ./outputs/platforms
INCLUDEPATH += $$DEPENDPATH

# use graphviz via library or executable?
packagesExist(libgvc) {
    DEFINES += USING_GRAPHVIZ_LIBS
    LIBS    += `pkg-config --libs libgvc`
    message("Linking to graphviz libraries")
} else {
    message("Using external graphviz binary (if available)")
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
install.files += browserconfig.xml
install.files += html/css html/fonts html/img html/js
INSTALLS      += install

# xmlpatterns module for xml validation
qtHaveModule(xmlpatterns) {
    QT += xmlpatterns
    DEFINES += USING_XMLPATTERNS
    HEADERS += torc/torcxmlvalidator.h
    SOURCES += torc/torcxmlvalidator.cpp
    message("QtXmlPatterns available")
}

# linux power support
linux:qtHaveModule(dbus) {
    QT += dbus
    HEADERS += torc/platforms/torcpowerunixdbus.h
    SOURCES += torc/platforms/torcpowerunixdbus.cpp
    DEFINES += USING_QTDBUS
    message("QtDBus available")
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
pi = $$(TORC_PI)
linux-rasp-pi-g++ | !isEmpty(pi) {
    LIBS += -lwiringPi
    DEFINES += USING_I2C
    DEFINES += USING_PIGPIO
    HEADERS += outputs/platforms/torci2cpca9685.h
    HEADERS += outputs/platforms/torcpigpio.h
    HEADERS += outputs/platforms/torcpiswitchoutput.h
    HEADERS += outputs/platforms/torcpipwmoutput.h
    HEADERS += inputs/platforms/torcpiswitchinput.h
    SOURCES += outputs/platforms/torci2cpca9685.cpp
    SOURCES += outputs/platforms/torcpigpio.cpp
    SOURCES += outputs/platforms/torcpiswitchoutput.cpp
    SOURCES += outputs/platforms/torcpipwmoutput.cpp
    SOURCES += inputs/platforms/torcpiswitchinput.cpp

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

# Bonjour is not available on windows
win32 {
    SOURCES += torc/platforms/torcbonjourwindows.cpp
    message("Bonjour NOT available")
} else {
    SOURCES += torc/torcbonjour.cpp
    message("Bonjour available")
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
HEADERS += torc/http/torchttprequest.h
HEADERS += torc/http/torchttpservice.h
HEADERS += torc/http/torchttpservices.h
HEADERS += torc/http/torchttpserver.h
HEADERS += torc/http/torchtmlhandler.h
HEADERS += torc/http/torchtmlstaticcontent.h
HEADERS += torc/http/torchtmldynamiccontent.h
HEADERS += torc/http/torchttphandler.h
HEADERS += torc/http/torchttpconnection.h
HEADERS += torc/http/torcwebsocket.h
HEADERS += torc/http/torcserialiser.h
HEADERS += torc/http/torcxmlserialiser.h
HEADERS += torc/http/torcjsonserialiser.h
HEADERS += torc/http/torcplistserialiser.h
HEADERS += torc/http/torcbinaryplistserialiser.h
HEADERS += torc/http/torcjsonrpc.h
HEADERS += torc/upnp/torcupnp.h
HEADERS += torc/upnp/torcssdp.h
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
HEADERS += control/torccontrol.h
HEADERS += control/torccontrols.h
HEADERS += control/torclogiccontrol.h
HEADERS += control/torctimercontrol.h
HEADERS += control/torctransitioncontrol.h
HEADERS += notify/torcnotify.h
HEADERS += notify/torcnotifier.h
HEADERS += notify/torclognotifier.h
HEADERS += notify/torcpushbulletnotifier.h
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
SOURCES += torc/http/torchttprequest.cpp
SOURCES += torc/http/torchttpserver.cpp
SOURCES += torc/http/torchtmlhandler.cpp
SOURCES += torc/http/torchtmlstaticcontent.cpp
SOURCES += torc/http/torchtmldynamiccontent.cpp
SOURCES += torc/http/torchttphandler.cpp
SOURCES += torc/http/torchttpconnection.cpp
SOURCES += torc/http/torchttpservice.cpp
SOURCES += torc/http/torchttpservices.cpp
SOURCES += torc/http/torcwebsocket.cpp
SOURCES += torc/http/torcserialiser.cpp
SOURCES += torc/http/torcxmlserialiser.cpp
SOURCES += torc/http/torcjsonserialiser.cpp
SOURCES += torc/http/torcplistserialiser.cpp
SOURCES += torc/http/torcbinaryplistserialiser.cpp
SOURCES += torc/http/torcjsonrpc.cpp
SOURCES += torc/upnp/torcupnp.cpp
SOURCES += torc/upnp/torcssdp.cpp
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
SOURCES += control/torccontrol.cpp
SOURCES += control/torccontrols.cpp
SOURCES += control/torclogiccontrol.cpp
SOURCES += control/torctimercontrol.cpp
SOURCES += control/torctransitioncontrol.cpp
SOURCES += notify/torcnotify.cpp
SOURCES += notify/torcnotifier.cpp
SOURCES += notify/torclognotifier.cpp
SOURCES += notify/torcpushbulletnotifier.cpp
SOURCES += notify/torcnotification.cpp
SOURCES += notify/torcsystemnotification.cpp
SOURCES += notify/torctriggernotification.cpp

HEADERS += torccentral.h
HEADERS += torcdevice.h
HEADERS += torcdevicehandler.h
HEADERS += torcxsdtest.h
SOURCES += main.cpp
SOURCES += torccentral.cpp
SOURCES += torcdevice.cpp
SOURCES += torcdevicehandler.cpp
SOURCES += torcxsdtest.cpp

QMAKE_CLEAN += $(TARGET)
