#check QT major version
lessThan(QT_MAJOR_VERSION, 5) {
    error("Must build against Qt5")
}

TEMPLATE    = app
CONFIG     += thread console
CONFIG     -=app_bundle
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

# libraries
LIBS       += -lz
!mac:LIBS  += -ldns_sd
!mac:LIBS  += -lrt

DEPENDPATH  += ./torc ./torc/http ./torc/upnp ./sensors ./sensors/platforms
DEPENDPATH  += ./outputs ./outputs/platforms
INCLUDEPATH += $$DEPENDPATH

# HTML
install.path   = $${PREFIX}/share/torc/html/
install.files  = html/index.html
install.files += html/css html/fonts html/img html/js
INSTALLS      += install

# linux power support
unix:qtHaveModule(dbus) {
    QT += dbus
    HEADERS += torc/platforms/torcpowerunixdbus.h
    SOURCES += torc/platforms/torcpowerunixdbus.cpp
    DEFINES += USING_QTDBUS
}

# I2c on the Raspberry Pi
linux-rasp-pi-g++ {
    LIBS += -lwiringPi
    DEFINES += USING_I2C
    DEFINES += USING_PIGPIO
    HEADERS += outputs/platforms/torci2cpca9685.h
    HEADERS += outputs/platforms/torcpigpio.h
    HEADERS += outputs/platforms/torcpioutput.h
    SOURCES += outputs/platforms/torci2cpca9685.cpp
    SOURCES += outputs/platforms/torcpigpio.cpp
    SOURCES += outputs/platforms/torcpioutput.cpp

    # install with suid permissions on Pi
    # this allows access to I2C and GPIO
    setpriv.target       = setpriv
    setpriv.depends      = FORCE
    setpriv.commands     = @echo set privileges for $$target.path/$$TARGET && chmod u+s $$target.path/$$TARGET
    QMAKE_EXTRA_TARGETS += setpriv
    setpriv.path         = .setpriv
    INSTALLS            += setpriv
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
HEADERS += torc/http/torchttprequest.h
HEADERS += torc/http/torchttpservice.h
HEADERS += torc/http/torchttpserver.h
HEADERS += torc/http/torchtmlhandler.h
HEADERS += torc/http/torchtmlserviceshelp.h
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
HEADERS += sensors/torcsensor.h
HEADERS += sensors/torcsensors.h
HEADERS += sensors/torcpwmsensor.h
HEADERS += sensors/torcswitchsensor.h
HEADERS += sensors/torcnetworkswitchsensor.h
HEADERS += sensors/torcnetworkpwmsensor.h
HEADERS += sensors/torcnetworksensors.h
HEADERS += sensors/torctemperaturesensor.h
HEADERS += sensors/platforms/torc1wirebus.h
HEADERS += sensors/platforms/torc1wireds18b20.h
HEADERS += outputs/torcoutput.h
HEADERS += outputs/torcoutputs.h
HEADERS += outputs/torcpwmoutput.h
HEADERS += outputs/torcswitchoutput.h
HEADERS += outputs/platforms/torci2cbus.h
HEADERS += control/torccontrol.h
HEADERS += control/torccontrols.h

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
SOURCES += torc/torcbonjour.cpp
SOURCES += torc/http/torchttprequest.cpp
SOURCES += torc/http/torchttpserver.cpp
SOURCES += torc/http/torchtmlhandler.cpp
SOURCES += torc/http/torchtmlserviceshelp.cpp
SOURCES += torc/http/torchtmlstaticcontent.cpp
SOURCES += torc/http/torchtmldynamiccontent.cpp
SOURCES += torc/http/torchttphandler.cpp
SOURCES += torc/http/torchttpconnection.cpp
SOURCES += torc/http/torchttpservice.cpp
SOURCES += torc/http/torcwebsocket.cpp
SOURCES += torc/http/torcserialiser.cpp
SOURCES += torc/http/torcxmlserialiser.cpp
SOURCES += torc/http/torcjsonserialiser.cpp
SOURCES += torc/http/torcplistserialiser.cpp
SOURCES += torc/http/torcbinaryplistserialiser.cpp
SOURCES += torc/http/torcjsonrpc.cpp
SOURCES += torc/upnp/torcupnp.cpp
SOURCES += torc/upnp/torcssdp.cpp
SOURCES += sensors/torcsensor.cpp
SOURCES += sensors/torcsensors.cpp
SOURCES += sensors/torcpwmsensor.cpp
SOURCES += sensors/torcswitchsensor.cpp
SOURCES += sensors/torctemperaturesensor.cpp
SOURCES += sensors/torcnetworkswitchsensor.cpp
SOURCES += sensors/torcnetworkpwmsensor.cpp
SOURCES += sensors/torcnetworksensors.cpp
SOURCES += sensors/platforms/torc1wirebus.cpp
SOURCES += sensors/platforms/torc1wireds18b20.cpp
SOURCES += outputs/torcoutput.cpp
SOURCES += outputs/torcoutputs.cpp
SOURCES += outputs/torcpwmoutput.cpp
SOURCES += outputs/torcswitchoutput.cpp
SOURCES += outputs/platforms/torci2cbus.cpp
SOURCES += control/torccontrol.cpp
SOURCES += control/torccontrols.cpp

HEADERS += torccentral.h
HEADERS += torcdevice.h
SOURCES += main.cpp
SOURCES += torccentral.cpp
SOURCES += torcdevice.cpp

QMAKE_CLEAN += $(TARGET)
