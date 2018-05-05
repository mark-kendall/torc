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
 * The default behaviour is to allow unlimited use of individual nonces and to expire them 10 seconds
 * after their issue OR last use.
 *
 *  \todo Investigate browsers that request a new nonce for every request
*/

// Currently at least, most interaction is via WebSockets, which are
// authorised once on creation.
// Some (older?) browsers seem to request a new nonce for every request - so again, try and limit the nonce
// count to something reasonable/manageable. (maybe an implementation issue that is causing the browser
// to re-issue)

bool TorcHTTPServerNonce::ProcessDigestAuth(TorcHTTPRequest *Request, bool WasStale, bool &Stale,
                                            const QString &Username, const QString &Password,
                                            const QString &Header, const QString &Method,
                                            const QString &URI)
{
    static QHash<QString,TorcHTTPServerNonce> nonces;
    static QByteArray token = QUuid::createUuid().toByteArray();
    static quint64 nonceCounter = 0;
    static QMutex lock(QMutex::Recursive);

    QDateTime current = QDateTime::currentDateTime();

    // Set digest authentication headers
    if (Request)
    {
        // try and build a unique nonce
        QByteArray tag = QByteArray::number(current.toMSecsSinceEpoch()) + Request->GetCache().toLocal8Bit() + token;
        QString nonce;

        {
            QMutexLocker locker(&lock);
            do
            {
                QByteArray hash(tag + QByteArray::number(++nonceCounter));
                nonce = QString(QCryptographicHash::hash(hash, QCryptographicHash::Md5).toHex());
            } while (nonces.contains(nonce));
        }

        TorcHTTPServerNonce nonceobj(current);
        lock.lock();
        nonces.insert(nonce, nonceobj);
        // NB SHA-256 doesn't seem to be implemented anywhere yet - so just offer MD5
        // should probably use insertMulti for SetResponseHeader
          QString auth = QString("Digest realm=\"%1\", qop=\"auth\", algorithm=MD5, nonce=\"%2\", opaque=\"%3\"%4")
                .arg(TORC_REALM)
                .arg(nonce)
                .arg(nonceobj.GetOpaque())
                .arg(WasStale ? QString(", stale=\"true\"") : QString(""));
        lock.unlock();

        LOG(VB_GENERAL, LOG_INFO, QString("New nonce: %1").arg(auth));
        LOG(VB_GENERAL, LOG_INFO, QString("Now %1 nonces").arg(nonces.size())); // not thread safe - do not leave

        Request->SetResponseHeader("WWW-Authenticate", auth);
    }
    // Check digest authentication
    else
    {
LOG(VB_GENERAL, LOG_INFO, QString("Before expire %1 nonces").arg(nonces.size())); // not thread safe - do not leave
        // expire old here. The authentication check is performed first (and on every request)
        {
            QMutexLocker locker(&lock);
            QMutableHashIterator<QString,TorcHTTPServerNonce> it(nonces);
            while (it.hasNext())
            {
                it.next();
                if (it.value().IsOutOfDate(current))
                    it.remove();
            }
        }
LOG(VB_GENERAL, LOG_INFO, QString("After expire %1 nonces").arg(nonces.size())); // not thread safe - do not leave
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

        if (hash3 != params.value("response"))
        {
            LOG(VB_GENERAL, LOG_WARNING, "Digest hash failed");
            return false;
        }

        // the uri MUST match the uri provided in the standard HTTP header, otherwise we could be authorise
        // access to the wrong resource
        if (URI != params.value("uri"))
        {
            LOG(VB_GENERAL, LOG_WARNING, "URI mismatch between HTTP request and WWW-Authenticate header");
            return false;
        }

        // we now don't need to check username, realm, password, method, URI or qop - as the hash check
        // would have failed. Likewise if algorithm was incorrect, we would have calculated the hash incorrectly.
        // The client has notionally verified its validity.

        {
            QMutexLocker locker(&lock);

            // find the nonce
            QHash<QString,TorcHTTPServerNonce>::const_iterator it = nonces.constFind(noncestr);
            if (it == nonces.constEnd())
            {
                LOG(VB_NETWORK, LOG_DEBUG, QString("Failed to find nonce '%1'").arg(noncestr));
                // if we got this far the nonce was valid but old, so set Stale and ask for re-auth
                Stale = true;
                return false;
            }

            TorcHTTPServerNonce nonce = it.value();

            // match opaque
            if (it.value().GetOpaque() != params.value("opaque"))
            {
                // this is an error
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
            if (!nonce.UseOnce(nc, Stale, current))
            {
                LOG(VB_NETWORK, LOG_DEBUG, "Nonce count use failed");
                return false;
            }
        }

LOG(VB_GENERAL, LOG_INFO, QString("Digest success"));

        return true; // joy unbounded
    }

    return false;
}

TorcHTTPServerNonce::TorcHTTPServerNonce()
  : m_expired(false),
    m_opaque(),
    m_startMs(0),
    m_startTime(),
    m_useCount(0),
    m_lifetimeInSeconds(0),
    m_lifetimeInRequests(0)
{
    LOG(VB_GENERAL, LOG_ERR, "Invalid TorcHTTPServerNonce");
}

TorcHTTPServerNonce::TorcHTTPServerNonce(const QDateTime &Time)
  : m_expired(false),
    m_opaque(QCryptographicHash::hash(QByteArray::number(qrand()), QCryptographicHash::Md5).toHex()),
    m_startMs(Time.time().msecsSinceStartOfDay()),
    m_startTime(Time),
    m_useCount(0),
    m_lifetimeInSeconds(DEFAULT_NONCE_LIFETIME_SECONDS),
    m_lifetimeInRequests(DEFAULT_NONCE_LIFETIME_REQUESTS)
{
}

TorcHTTPServerNonce::~TorcHTTPServerNonce()
{
}

QString TorcHTTPServerNonce::GetOpaque(void) const
{
    return m_opaque;
}

bool TorcHTTPServerNonce::UseOnce(quint64 ClientCount, bool &Stale, const QDateTime &Current)
{
    m_useCount++;

    // the nc value is designed to prevent replay attacks. Hence we should only ever see the same nc
    // value once. In theory it should start at 1 and increase monotonically. In practice, we may miss
    // requests and/or they may arrive out of order.
    // We make no attempt to track individual nc values or out of order requests. If the nc value
    // is greater than or equal to the expected value, we allow it. Otherwise we set Stale to true,
    // which will trigger the client to re-authenticate.

    // I can't see any reference on how to deal with nonce count wrapping (even though highly unlikely)
    // so assume it wraps to 1... max value is 8 character hex i.e. ffffffff or 32bit integer. For safety,
    // invalidate the nonce.
    if (m_useCount > 0xffffffff)
    {
        m_expired = true;
        Stale = true;
        m_useCount = 1;
        return false;
    }

    if (m_useCount <= ClientCount)
    {
        // update to the client's count. This MAY invalidate out of order requests.
        m_useCount = ClientCount;

        // check for total usage (zero request lifetime implies unlimited usage)
        if (m_lifetimeInRequests > 0 && m_useCount >= m_lifetimeInRequests)
        {
            m_expired = true;
            Stale = true;
            return false;
        }

        // keep the nonce alive
        m_startTime = Current;
        return true;
    }

    // unexpected nc value (less than expected)
    m_expired = true;
    Stale = true;
    return false;
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
