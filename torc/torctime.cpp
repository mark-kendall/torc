/* Class TorcTime
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torctime.h"
#include "torclogging.h"
#include "torclocalcontext.h"
#include "torcadminthread.h"

/*! \class TorcTime
 *  \brief A simple time service to confirm the time (and date) the system is currently operating with.
 *
 * Useful to confirm to the remote user that the system is operating as expected and also acts as a
 * heartbeat over the websocket connection to the remote interface.
 *
 *  \todo Allow customisation of the time format.
*/
TorcTime::TorcTime()
  : QObject(), TorcHTTPService(this, "time", "time", TorcTime::staticMetaObject, "Tick"),
    currentTime(),
    m_lastTime(QDateTime::currentDateTime()),
    m_timer(),
    m_dateTimeFormat(),
    m_timeLock()
{
    // NB if the language changes, the device will be restarted so no need to worry about format changes
    m_dateTimeFormat = gLocalContext->GetLocale().dateTimeFormat(QLocale::LongFormat);
    m_timer.setInterval(1000);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(Tick()));
    m_timer.start();
}

TorcTime::~TorcTime()
{
}

QString TorcTime::GetCurrentTime(void)
{
    QReadLocker locker(&m_timeLock);
    return m_lastTime.toString(m_dateTimeFormat);
}

QString TorcTime::GetCurrentTimeUTC(void)
{
    QReadLocker locker(&m_timeLock);
    return m_lastTime.toUTC().toString(Qt::ISODate);
}

void TorcTime::Tick(void)
{
    QWriteLocker locker(&m_timeLock);
    QDateTime timenow = QDateTime::currentDateTime();
    qint64 difference = timenow.secsTo(m_lastTime);
    m_lastTime = timenow;
    if (qAbs(difference) > 10)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Detected change in system time");
        gLocalContext->NotifyEvent(Torc::SystemTimeChanged);
    }

    emit currentTimeChanged(m_lastTime.toString(m_dateTimeFormat));
}

void TorcTime::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \class TorcTimeObject
 *  \brief A static class used to create the TorcTime singleton in the admin thread.
*/
static class TorcTimeObject : public TorcAdminObject
{
  public:
    TorcTimeObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY),
        m_time(NULL)
    {
    }

    virtual ~TorcTimeObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();
        m_time = new TorcTime();
    }

    void Destroy(void)
    {
        if (m_time)
            delete m_time;
        m_time = NULL;
    }

  private:
    Q_DISABLE_COPY(TorcTimeObject)
    TorcTime *m_time;

} TorcTimeObject;
