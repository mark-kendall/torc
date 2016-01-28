/*! \mainpage Torc
 *
 * \section intro Introduction
 * Torc is an open source, cross-platform automation application.
 *
 * \section torctoc Content
 * -# \ref pi
 *
 * \page pi Torc on the Raspberry Pi
 * \section pitorc Introduction
 *
 * Torc was originally written for installation on a Model B Raspberry Pi but will work on any
 * version of the hardware. A full desktop installation is not required but a network connection is essential (Torc is
 * accessed through a web interface).
 *
 * \note The following instructions are not exhaustive and additional steps may be required for distributions other
 * than Raspbian Jessie.
 *
 * \section piinstall Installing on the Raspberry Pi.
 *
 * \subsection pidownload Download and install linux.
 *  Download the latest version of your favourite Raspberry Pi linux image (the following instructions
 * assume Raspbian Jesie Lite - but any recent distribution should work). Torc does not require a desktop
 * interface so a minimal (headless) version is fine. Install that image on a suitable SD card and insert the card
 * into your device.
 *
 * \subsection piboot Boot device and log in.
 * Boot your device and log in. If you are running a full desktop insallation, open a command terminal -
 * the following assumes you are installing from the command line.
 *
 * \subsection piupdate Update your installation.
 * Update your distribution for all of the latest fixes and releases by typing the following commands
 * (depending on the number of updates, this may take a few minutes):
 * \code
 * sudo apt-get update
 * sudo apt-get upgrade
 * \endcode
 *
 * \subsection piconfig Change your Raspberry Pi configuration.
 * From the command line, run:
 * \code
 * sudo raspi-config
 * \endcode
 *
 * You can change a number of Raspberry Pi specific behaviours from this utility. You will probably
 * want to 'Expand filesystem' (to make sure the entire SD card is available to you), 'Change user password',
 * set the timezone (IMPORTANT) and from the 'Advanced options' screen, 'Set the hostname', 'Enable SSH', 'Enable SPI' and 'Enable I2C'.
 * The last 2 options are dependant on the hardware you intend to attach to your device. If you have no intention of attaching
 * a screen or monitor to your device, you can also allocate zero memory to the GPU.
 *
 * \subsection pidev Install software dependencies.
 *
 * From the command line. run:
 * \code
 * sudo apt-get install libgraphviz-dev libavahi-compat-libdnssd-dev qt5-default git-core
 * \endcode
 *
 * \subsection piwiringpi Install wiringPi.
 *
 * Torc uses wiringPi to access the Raspberry Pi's GPIO pins and help with I2C communications. It is highly unlikely
 * that you'll want to use Torc on a Pi without wiringPi. Full wiringPi instructions are available at the wiringPi
 * \link http://wiringpi.com/download-and-install project page.\endlink
 *
 * \code
 * cd ~
 * git clone git://git.drogon.net/wiringPi
 * cd wiringPi
 * ./build
 * \endcode
 *
 * \subsection pitorcinstall Download and install Torc.
 * \note This will take some time. Make a few cups of coffee...
 * \code
 * cd ~
 * git clone https://github.com/mark-kendall/torc.git
 * cd torc
 * qmake
 * make
 * sudo make install
 * \endcode
 */
