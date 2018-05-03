/* Class TorcHTTPServerNonce
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
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

// Qt
#include <QUuid>
#include <QCryptographicHash>

// Torc
#include "torclogging.h"
#include "torchttpservernonce.h"

/*! \brief A server nonce for Digest Access Authentication
 *
 *  \todo Use single use nonces for PUT/POST requests
 *  \todo Investigate browsers that request a new nonce for every request
 *  \todo Implement auth-int? (very confusing what the entity body is - and may not be performant)
*/

// We restrict the nonce life to 1 minute by default. This minimises the number of 'live' nonces
// that we need to track and, currently at least, most interaction is via WebSockets, which are
// authorised once on creation.
// Some (older?) browsers seem to request a new nonce for every request - so again, try and limit the nonce
// count to something reasonable/manageable. (maybe an implementation issue that is causing the browser
// to re-issue)

quint64                         TorcHTTPServerNonce::gServerNoncesCount = 0;
QHash<QString,TorcHTTPServerNonce*> TorcHTTPServerNonce::gServerNonces;
QMutex*                         TorcHTTPServerNonce::gServerNoncesLock = new QMutex(QMutex::Recursive);

static const QUuid kServerToken = QUuid::createUuid();

bool TorcHTTPServerNonce::AddNonce(TorcHTTPServerNonce* Nonce)
{
    if (Nonce)
    {
        QMutexLocker locker(gServerNoncesLock);
        if (!gServerNonces.contains(Nonce->GetNonce()))
        {
            gServerNonces.insert(Nonce->GetNonce(), Nonce);
            return true;
        }
    }
    return false;
}

void TorcHTTPServerNonce::RemoveNonce(TorcHTTPServerNonce* Nonce)
{
    if (Nonce)
    {
        QMutexLocker locker(gServerNoncesLock);
        gServerNonces.remove(Nonce->GetNonce());
    }
}

void TorcHTTPServerNonce::ExpireNonces(void)
{
    QMutexLocker locker(gServerNoncesLock);

    QList<TorcHTTPServerNonce*> oldnonces;
    QDateTime current = QDateTime::currentDateTime();

    QHash<QString,TorcHTTPServerNonce*>::const_iterator it = gServerNonces.constBegin();
    for ( ; it != gServerNonces.constEnd(); ++it)
        if (it.value()->IsOutOfDate(current))
            oldnonces.append(it.value());

    foreach (TorcHTTPServerNonce* nonce, oldnonces)
        delete nonce;
}

bool TorcHTTPServerNonce::CheckAuthentication(const QString &Username, const QString &Password,
                                              const QString &Header,   const QString &Method,
                                              const QString &URI,      bool &Stale)
{
    // remove leading 'Digest' and split out parameters
    QStringList authentication = Header.mid(6).trimmed().split(",", QString::SkipEmptyParts);

    // create a filtered hash of the parameters
    QHash<QString,QString> params;
    foreach (QString auth, authentication)
    {
        // various parameters can contain an '=' in the body, so only search for the first '='
        QString key   = auth.section('=', 0, 0).trimmed().toLower();
        QString value = auth.section('=', 1).trimmed();
        value.remove("\"");
        params.insert(key, value);
    }

    // we need username, realm, nonce, uri, qop, algorithm, nc, cnonce, response and opaque...
    if (params.size() < 10)
    {
        LOG(VB_NETWORK, LOG_DEBUG, "Digest response received too few parameters");
        return false;
    }

    // check for presence of each
    if (!params.contains("username") || !params.contains("realm") || !params.contains("nonce") ||
        !params.contains("uri") || !params.contains("qop") || !params.contains("algorithm") ||
        !params.contains("nc") || !params.contains("cnonce") || !params.contains("response") ||
        !params.contains("opaque"))
    {
        LOG(VB_NETWORK, LOG_DEBUG, "Did not receive expected paramaters");
        return false;
    }

    // we check the digest early, even though it is an expensive operation, as it will automatically
    // confirm a number of the params are correct and if the respone is calculated correctly but the nonce
    // is not recognised (i.e. stale), we can respond with stale="true', as per the spec, and the client
    // can resubmit without prompting the user for credentials (i.e. the client has proved it knows the correct
    // credentials but the nonce is out of date)
    QString  noncestr = params.value("nonce");
    QString    ncstr  = params.value("nc");
    QString    first  = QString("%1:%2:%3").arg(Username).arg(TORC_REALM).arg(Password);
    QString    second = QString("%1:%2").arg(Method).arg(URI);
    QByteArray hash1  = QCryptographicHash::hash(first.toLatin1(), QCryptographicHash::Md5).toHex();
    QByteArray hash2  = QCryptographicHash::hash(second.toLatin1(), QCryptographicHash::Md5).toHex();
    QString    third  = QString("%1:%2:%3:%4:%5:%6").arg(QString(hash1)).arg(noncestr).arg(ncstr).arg(params.value("cnonce")).arg("auth").arg(QString(hash2));
    QByteArray hash3  = QCryptographicHash::hash(third.toLatin1(), QCryptographicHash::Md5).toHex();

    bool digestcorrect = false;
    if (hash3 == params.value("response"))
    {
        LOG(VB_NETWORK, LOG_DEBUG, "Digest hash matched");
        digestcorrect = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Digest hash failed");
        return false;
    }

    // we now don't need to check username, realm, password, method, URI or qop - as the hash check
    // would have failed. Likewise if algorithm was incorrect, we would calculated the hash incorrectly.

    {
        QMutexLocker locker(gServerNoncesLock);

        // find the nonce
        QHash<QString,TorcHTTPServerNonce*>::const_iterator it = gServerNonces.constFind(noncestr);
        if (it == gServerNonces.constEnd())
        {
            LOG(VB_NETWORK, LOG_DEBUG, QString("Failed to find nonce '%1'").arg(noncestr));
            // set stale to true if the digest calc worked
            Stale = digestcorrect;
            return false;
        }

        TorcHTTPServerNonce* nonce = it.value();

        // match opaque
        if (nonce->GetOpaque() != params.value("opaque"))
        {
            LOG(VB_NETWORK, LOG_DEBUG, "Failed to match opaque");
            return false;
        }

        // parse nonse count from hex
        bool ok = false;
        quint64 nc = ncstr.toInt(&ok, 16);
        if (!ok)
        {
            LOG(VB_NETWORK, LOG_DEBUG, "Failed to parse nonce count");
            return false;
        }

        // check nonce count
        if (!nonce->UseOnce(nc, Stale))
        {
            LOG(VB_NETWORK, LOG_DEBUG, "Nonce count use failed");
            return false;
        }
    }

    return true; // joy unbounded
}

TorcHTTPServerNonce::TorcHTTPServerNonce()
  : m_expired(false),
    m_nonce(QByteArray()),
    m_opaque(QCryptographicHash::hash(QByteArray::number(qrand()), QCryptographicHash::Md5).toHex()),
    m_startMs(QTime::currentTime().msecsSinceStartOfDay()),
    m_startTime(QDateTime::currentDateTime()),
    m_useCount(0),
    m_lifetimeInSeconds(DEFAULT_NONCE_LIFETIME_SECONDS),
    m_lifetimeInRequests(0)
{
    CreateNonce();
}

TorcHTTPServerNonce::TorcHTTPServerNonce(quint64 LifetimeInSeconds, quint64 LifetimeInRequests)
  : m_expired(false),
    m_nonce(QByteArray()),
    m_opaque(QCryptographicHash::hash(QByteArray::number(qrand()), QCryptographicHash::Md5).toHex()),
    m_startMs(QTime::currentTime().msecsSinceStartOfDay()),
    m_startTime(QDateTime::currentDateTime()),
    m_useCount(0),
    m_lifetimeInSeconds(LifetimeInSeconds),
    m_lifetimeInRequests(LifetimeInRequests)
{
    CreateNonce();
}

TorcHTTPServerNonce::~TorcHTTPServerNonce()
{
    RemoveNonce(this);
}

QString TorcHTTPServerNonce::GetNonce(void) const
{
    return m_nonce;
}

QString TorcHTTPServerNonce::GetOpaque(void) const
{
    return m_opaque;
}

bool TorcHTTPServerNonce::UseOnce(quint64 ThisUse, bool &Stale)
{
    m_useCount++;

    // I can't see any reference on how to deal with nonce count wrapping (even though highly unlikely)
    // so assume it wraps to 1... max value is 8 character hex i.e. ffffffff or 32bit integer.
    if (m_useCount > 0xffffffff)
        m_useCount = 1;

    bool result = m_useCount <= ThisUse;
    if (result == false)
        Stale = true;

    // allow the client count to run ahead but reset our count if needed
    // this may cause issues if requests are received out of order but the client will just re-submit
    if (ThisUse > m_useCount)
        m_useCount = ThisUse;

    // check for expiry request lifetime expiry
    // we check for timed expiry in IsOutOfDate
    if ((m_lifetimeInRequests > 0) && (m_useCount >= m_lifetimeInRequests))
    {
        m_expired = true;
        Stale     = true;
        result    = false;
    }

    return result;
}

bool TorcHTTPServerNonce::IsOutOfDate(const QDateTime &Current)
{
    // request lifetime is checked when actually used, so only check time here
    // this is the main mechanism for managing the size of the nonce list
    if (!m_expired)
        if (Current > m_startTime.addSecs(m_lifetimeInSeconds))
            m_expired = true;
    return m_expired;
}

void TorcHTTPServerNonce::CreateNonce(void)
{
    // remove the old and the frail (before we add this nonce)
    ExpireNonces();

    // between the date time, IP and nonce count, we should get a unique nonce - but
    // double check and increment the nonce count on fail.
    do
    {
        QByteArray hash(QByteArray::number(m_lifetimeInRequests) +
                        QByteArray::number(m_lifetimeInSeconds) +
                        kServerToken.toByteArray() +
                        QByteArray::number(m_startMs) +
                        m_startTime.toString().toLatin1() +
                        QByteArray::number(++gServerNoncesCount));
        m_nonce = QString(QCryptographicHash::hash(hash, QCryptographicHash::Md5).toHex());
    } while (!AddNonce(this));
}
