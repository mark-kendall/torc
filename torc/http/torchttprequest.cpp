/* Class TorcHTTPRequest
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
#include <QScopedPointer>
#include <QTcpSocket>
#include <QTextStream>
#include <QStringList>
#include <QDateTime>
#include <QRegExp>
#include <QFile>
#include <QUrl>

// Torc
#include "torccompat.h"
#include "torclocaldefs.h"
#include "torclogging.h"
#include "torccoreutils.h"
#include "torcmime.h"
#include "torchttpserver.h"
#include "torcserialiser.h"
#include "torcjsonserialiser.h"
#include "torcxmlserialiser.h"
#include "torcplistserialiser.h"
#include "torcbinaryplistserialiser.h"
#include "torchttprequest.h"

#if defined(Q_OS_LINUX)
#include <sys/sendfile.h>
#include <sys/errno.h>
#elif defined(Q_OS_MAC)
#include <sys/socket.h>
#endif

/*! \class TorcHTTPRequest
 *  \brief A class to encapsulate an incoming HTTP request.
 *
 * TorcHTTPRequest validates an incoming HTTP request and prepares the appropriate
 * headers for the response.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 *
 * \todo Add support for multiple headers of the same type (e.g. Sec-WebSocket-Protocol).
 * \todo Support gzip compression for range requests (if it is possible?)
*/

QRegExp gRegExp = QRegExp("[ \r\n][ \r\n]*");
char TorcHTTPRequest::DateFormat[] = "ddd, dd MMM yyyy HH:mm:ss 'UTC'";

TorcHTTPRequest::TorcHTTPRequest(TorcHTTPReader *Reader)
  : m_fullUrl(),
    m_path(),
    m_method(),
    m_query(),
    m_redirectedTo(),
    m_type(HTTPRequest),
    m_requestType(HTTPUnknownType),
    m_protocol(HTTPUnknownProtocol),
    m_connection(HTTPConnectionClose),
    m_ranges(),
    m_headers(),
    m_queries(),
    m_content(),
    m_secure(false),
    m_allowGZip(false),
    m_allowCORS(false),
    m_allowed(0),
    m_authorised(HTTPNotAuthorised),
    m_responseType(HTTPResponseUnknown),
    m_cache(HTTPCacheNone),
    m_cacheTag(QStringLiteral("")),
    m_responseStatus(HTTP_NotFound),
    m_responseContent(),
    m_responseFile(),
    m_responseHeaders()
{
    if (Reader)
    {
        Reader->TakeRequest(m_content, m_headers);
        Initialise(Reader->GetMethod());
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("NULL Reader"));
    }
}

void TorcHTTPRequest::Initialise(const QString &Method)
{
    QStringList items = Method.split(gRegExp, QString::SkipEmptyParts);
    QString item;

    if (!items.isEmpty())
    {
        item = items.takeFirst();

        // response of type 'HTTP/1.1 200 OK'
        if (item.startsWith(QStringLiteral("HTTP")))
        {
            m_type = HTTPResponse;

            // HTTP/1.1
            m_protocol = ProtocolFromString(item.trimmed());

            // 200 OK
            if (!items.isEmpty())
                m_responseStatus = StatusFromString(items.takeFirst().trimmed());
        }
        // request of type 'GET /method HTTP/1.1'
        else
        {
            m_type = HTTPRequest;

            // GET
            m_requestType = RequestTypeFromString(item.trimmed());

            if (!items.isEmpty())
            {
                // /method
                QUrl url  = QUrl::fromEncoded(items.takeFirst().toUtf8());
                m_path    = url.path();
                m_fullUrl = url.toString();

                int index = m_path.lastIndexOf('/');
                if (index > -1)
                {
                    m_method = m_path.mid(index + 1).trimmed();
                    m_path   = m_path.left(index + 1).trimmed();
                }

                if (url.hasQuery())
                {
                    QStringList pairs = url.query().split('&');
                    foreach (const QString &pair, pairs)
                    {
                        int index = pair.indexOf('=');
                        QString key = pair.left(index);
                        QString val = pair.mid(index + 1);
                        m_queries.insert(key, val);
                    }
                }
            }

            // HTTP/1.1
            if (!items.isEmpty())
                m_protocol = ProtocolFromString(items.takeFirst());
        }
    }

    if (m_protocol > HTTPOneDotZero)
        m_connection = HTTPConnectionKeepAlive;

    QString connection = m_headers.value(QStringLiteral("Connection")).toLower();

    if (connection == QStringLiteral("keep-alive"))
        m_connection = HTTPConnectionKeepAlive;
    else if (connection == QStringLiteral("close"))
        m_connection = HTTPConnectionClose;

    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("HTTP request: path '%1' method '%2'").arg(m_path, m_method));
}

void TorcHTTPRequest::SetConnection(HTTPConnection Connection)
{
    m_connection = Connection;
}

void TorcHTTPRequest::SetStatus(HTTPStatus Status)
{
    m_responseStatus = Status;
}

void TorcHTTPRequest::SetResponseType(HTTPResponseType Type)
{
    m_responseType = Type;
}

void TorcHTTPRequest::SetResponseContent(const QByteArray &Content)
{
    m_responseFile    = QStringLiteral();
    m_responseContent = Content;
}

void TorcHTTPRequest::SetResponseFile(const QString &File)
{
    m_responseFile    = File;
    m_responseContent = QByteArray();
}

void TorcHTTPRequest::SetResponseHeader(const QString &Header, const QString &Value)
{
    m_responseHeaders.insert(Header, Value);
}

void TorcHTTPRequest::SetAllowed(int Allowed)
{
    m_allowed = Allowed;
}

///\brief Allow gzip compression for the contents of this request.
void TorcHTTPRequest::SetAllowGZip(bool Allowed)
{
    m_allowGZip = Allowed;
}

void TorcHTTPRequest::SetAllowCORS(bool Allowed)
{
    m_allowCORS = Allowed;
}

bool TorcHTTPRequest::GetAllowCORS(void) const
{
    return m_allowCORS;
}

/*! \brief Set the caching behaviour for this response.
 *
 * The default behaviour is to to try and enforce no caching. Standard caching can be enabled
 * with HTTPCacheShortLife or HTTPCacheLongLife with optional use of 'last modified' or ETag for
 * conditional requests. The 'last-modified' and 'ETag' fields are set with the Tag parameter.
 *
 * \note If a subclass of TorcHTTPHandler uses the 'last-modified' or 'ETag' headers, it must also
 * be capable of handling the appropriate conditional requests and responding with a '304 Not Modified' as necessary.
 */
void TorcHTTPRequest::SetCache(int Cache, const QString &Tag /* = QString("")*/)
{
    m_cache = Cache;
    m_cacheTag = Tag;
}

void TorcHTTPRequest::SetSecure(bool Secure)
{
    m_secure = Secure;
}

bool TorcHTTPRequest::GetSecure(void)
{
    return m_secure;
}

HTTPStatus TorcHTTPRequest::GetHTTPStatus(void) const
{
    return m_responseStatus;
}

HTTPType TorcHTTPRequest::GetHTTPType(void) const
{
    return m_type;
}

HTTPRequestType TorcHTTPRequest::GetHTTPRequestType(void) const
{
    return m_requestType;
}

HTTPProtocol TorcHTTPRequest::GetHTTPProtocol(void) const
{
    return m_protocol;
}

QString TorcHTTPRequest::GetUrl(void) const
{
    return m_fullUrl;
}

QString TorcHTTPRequest::GetPath(void) const
{
    return m_path;
}

QString TorcHTTPRequest::GetMethod(void) const
{
    return m_method;
}

QString TorcHTTPRequest::GetCache(void) const
{
    return m_cacheTag;
}

const QMap<QString,QString>& TorcHTTPRequest::Headers(void) const
{
    return m_headers;
}

const QMap<QString,QString>& TorcHTTPRequest::Queries(void) const
{
    return m_queries;
}

void TorcHTTPRequest::Respond(QTcpSocket *Socket)
{
    if (!Socket)
        return;

    QFile file;
    if (!m_responseFile.isEmpty())
        file.setFileName(m_responseFile);

    // this is not the file you are looking for...
    if ((m_responseType == HTTPResponseUnknown && m_responseFile.isEmpty()) || m_responseStatus == HTTP_NotFound)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("'%1' not found").arg(m_fullUrl));
        m_responseStatus  = HTTP_NotFound;

        QByteArray result;
        QTextStream stream(&result);
        stream << "<html><head><title>" << TORC_REALM << "</title></head>";
        stream << "<body><h1><a href='/'>" << TORC_REALM << "</a></h1>";
        stream << "<p>File not found";
        stream << "</body></html>";
        this->SetResponseContent(result);
        this->SetResponseType(HTTPResponseHTML);
    }

    QString contenttype = ResponseTypeToString(m_responseType);

    // set the response type based upon file
    if (!m_responseFile.isEmpty())
    {
        // only use the file contents if the file name returns an ambiguous result
        QStringList types = TorcMime::MimeTypeForFileName(m_responseFile);
        if (types.size() == 1)
            contenttype = types.first();
        else
            contenttype = TorcMime::MimeTypeForFileNameAndData(m_responseFile, &file);
        m_responseType = HTTPResponseDefault;
    }

    QByteArray contentheader = QStringLiteral("Content-Type: %1\r\n").arg(contenttype).toLatin1();

    // process byte range requests
    qint64 totalsize  = !m_responseContent.isEmpty() ? m_responseContent.size() : !m_responseFile.isEmpty() ? file.size() : 0;
    qint64 sendsize   = totalsize;
    bool multipart    = false;
    static QByteArray seperator("\r\n--STaRT\r\n");
    QList<QByteArray> partheaders;

    if (m_headers.contains(QStringLiteral("Range")) && m_responseStatus == HTTP_OK)
    {
        m_ranges = StringToRanges(m_headers.value(QStringLiteral("Range")), totalsize, sendsize);

        if (m_ranges.isEmpty())
        {
            QByteArray empty;
            SetResponseContent(empty);
            m_responseStatus = HTTP_RequestedRangeNotSatisfiable;
        }
        else
        {
            m_responseStatus = HTTP_PartialContent;
            multipart = m_ranges.size() > 1;

            if (multipart)
            {
                QVector<QPair<quint64,quint64> >::const_iterator it = m_ranges.constBegin();
                for ( ; it != m_ranges.constEnd(); ++it)
                {
                    QByteArray header = seperator + contentheader + QStringLiteral("Content-Range: bytes %1\r\n\r\n").arg(RangeToString((*it), totalsize)).toLatin1();
                    partheaders << header;
                    sendsize += header.size();
                }
            }
        }
    }

    // format response headers
    QScopedPointer<QByteArray> headers(new QByteArray);
    QTextStream response(headers.data());

    response << TorcHTTPRequest::ProtocolToString(m_protocol) << " " << TorcHTTPRequest::StatusToString(m_responseStatus) << "\r\n";
    response << "Date: " << QDateTime::currentDateTimeUtc().toString(DateFormat) << "\r\n";
    response << "Server: " << TorcHTTPServer::PlatformName() << "\r\n";
    response << "Connection: " << TorcHTTPRequest::ConnectionToString(m_connection) << "\r\n";
    response << "Accept-Ranges: bytes\r\n";

    if (m_cache & HTTPCacheNone)
    {
        response << "Cache-Control: private, no-cache, no-store, must-revalidate\r\nExpires: 0\r\nPragma: no-cache\r\n";
    }
    else
    {
        // cache-control (in preference to expires for its simplicity)
        if (m_cache & HTTPCacheShortLife)
            response << "Cache-Control: public, max-age=3600\r\n"; // 1 hour
        else if (m_cache & HTTPCacheLongLife)
            response << "Cache-Control: public, max-age=31536000\r\n"; // 1 year (max per spec)

        // either last-modified or etag (not both) if requested
        if (!m_cacheTag.isEmpty())
        {
            if (m_cache & HTTPCacheETag)
                response << QStringLiteral("ETag: \"%1\"\r\n").arg(m_cacheTag);
            else if (m_cache & HTTPCacheLastModified)
                response << QStringLiteral("Last-Modified: %1\r\n").arg(m_cacheTag);
        }
    }

    // Use compression if:-
    //  - it was requested by the client.
    //  - zlip support is available locally.
    //  - the responder allows gzip responses.
    //  - there is some content and it is smaller than 1Mb in size (arbitrary limit)
    //  - the response is not a range request with single or multipart response
    if (m_allowGZip && totalsize > 0 && totalsize < 0x100000 && TorcCoreUtils::HasZlib() && m_responseStatus == HTTP_OK &&
        m_headers.contains(QStringLiteral("Accept-Encoding")) &&
            m_headers.value(QStringLiteral("Accept-Encoding")).contains(QStringLiteral("gzip"), Qt::CaseInsensitive))
    {
        if (!m_responseContent.isEmpty())
        {
            QByteArray newcontent = TorcCoreUtils::GZipCompress(m_responseContent);
            SetResponseContent(newcontent);
        }
        else if (!m_responseFile.isEmpty())
        {
            QByteArray newcontent = TorcCoreUtils::GZipCompressFile(file);
            SetResponseContent(newcontent);
        }
        sendsize = m_responseContent.size();
        response << "Content-Encoding: gzip\r\n";
    }

    if (multipart)
        response << "Content-Type: multipart/byteranges; boundary=STaRT\r\n";
    else if (m_responseType != HTTPResponseNone)
        response << contentheader;

    if (m_allowed)
        response << "Allow: " << AllowedToString(m_allowed) << "\r\n";
    response << "Content-Length: " << QString::number(sendsize) << "\r\n";

    if (m_responseStatus == HTTP_PartialContent && !multipart)
        response << "Content-Range: bytes " << RangeToString(m_ranges[0], totalsize) << "\r\n";
    else if (m_responseStatus == HTTP_RequestedRangeNotSatisfiable)
        response << "Content-Range: bytes */" << QString::number(totalsize) << "\r\n";

    if (m_responseStatus == HTTP_MovedPermanently)
        response << "Location: " << m_redirectedTo.toLatin1() << "\r\n";

    // process any custom headers
    QMap<QString,QString>::const_iterator it = m_responseHeaders.constBegin();
    for ( ; it != m_responseHeaders.constEnd(); ++it)
        response << it.key().toLatin1() << ": " << it.value().toLatin1() << "\r\n";

    response << "\r\n";
    response.flush();

    // send headers
    qint64 headersize = headers.data()->size();
    qint64 sent = Socket->write(headers.data()->constData(), headersize);
    if (headersize != sent)
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Buffer size %1 - but sent %2").arg(headersize).arg(sent));
    else
        LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 header bytes").arg(sent));

    LOG(VB_NETWORK, LOG_DEBUG, QString(headers->data()));

    // send content
    if (!m_responseContent.isEmpty() && m_requestType != HTTPHead)
    {
        if (multipart)
        {
            QVector<QPair<quint64,quint64> >::const_iterator it = m_ranges.constBegin();
            QList<QByteArray>::const_iterator bit = partheaders.constBegin();
            for ( ; it != m_ranges.constEnd(); ++it, ++bit)
            {
                qint64 sent = Socket->write((*bit).data(), (*bit).size());
                if ((*bit).size() != sent)
                    LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Buffer size %1 - but sent %2").arg((*bit).size()).arg(sent));
                else
                    LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 multipart header bytes").arg(sent));

                quint64 start      = (*it).first;
                qint64 chunksize  = (*it).second - start + 1;
                sent  = Socket->write(m_responseContent.constData() + start, chunksize);
                if (chunksize != sent)
                    LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Buffer size %1 - but sent %2").arg(chunksize).arg(sent));
                else
                    LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 content bytes").arg(sent));
            }
        }
        else
        {
            qint64 size = sendsize;
            qint64 offset = m_ranges.isEmpty() ? 0 : m_ranges.value(0).first;
            qint64 sent = Socket->write(m_responseContent.constData() + offset, size);
            if (size != sent)
                LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Buffer size %1 - but sent %2").arg(size).arg(sent));
            else
                LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 content bytes").arg(sent));
        }
    }
    else if (!m_responseFile.isEmpty() && m_requestType != HTTPHead)
    {
        if (!file.isOpen())
            file.open(QIODevice::ReadOnly);

        if (multipart)
        {
            QScopedPointer<QByteArray> buffer(new QByteArray(READ_CHUNK_SIZE, 0));

            QVector<QPair<quint64,quint64> >::const_iterator it = m_ranges.constBegin();
            QList<QByteArray>::const_iterator bit = partheaders.constBegin();
            for ( ; it != m_ranges.constEnd(); ++it, ++bit)
            {
                off64_t sent = Socket->write((*bit).data(), (*bit).size());
                if ((*bit).size() != sent)
                    LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Buffer size %1 - but sent %2").arg((*bit).size()).arg(sent));
                else
                    LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 multipart header bytes").arg(sent));

                sent   = 0;
                off64_t offset = (*it).first;
                off64_t size   = (*it).second - offset + 1;

#if defined(Q_OS_LINUX)
                if (size > sent)
                {
                    // sendfile64 accesses the socket directly, bypassing Qt's cache, so we must flush first
                    Socket->flush();

                    do
                    {
                        off64_t remaining = qMin(size - sent, (off64_t)READ_CHUNK_SIZE);
                        off64_t send = sendfile64(Socket->socketDescriptor(), file.handle(), &offset, remaining);

                        if (send < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                QThread::usleep(5000);
                                continue;
                            }

                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending data (%1) %2").arg(errno).arg(strerror(errno)));
                            break;
                        }
                        else
                        {
                            LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 for %2").arg(send).arg(file.handle()));
                        }

                        sent += send;
                    }
                    while (sent < size);
                }
#elif defined(Q_OS_MAC)
                if (size > sent)
                {
                    // sendfile accesses the socket directly, bypassing Qt's cache, so we must flush first
                    Socket->flush();

                    off64_t bytessent  = 0;
                    off64_t off        = offset;

                    do
                    {
                        bytessent = qMin(size - sent, (qint64)READ_CHUNK_SIZE);
                        if (sendfile(file.handle(), Socket->socketDescriptor(), off, &bytessent, nullptr, 0) < 0)
                        {
                            if (errno != EAGAIN)
                            {
                                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending data (%1) %2").arg(errno).arg(strerror(errno)));
                                break;
                            }

                            QThread::usleep(5000);
                        }
                        else
                        {
                            LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Sent %1 for %2").arg(bytessent).arg(file.handle()));
                        }

                        sent += bytessent;
                        off  += bytessent;
                    }
                    while (sent < size);
                }
#else
                if (size > sent)
                {
                    file.seek(offset);

                    do
                    {
                        qint64 remaining = qMin(size - sent, (qint64)READ_CHUNK_SIZE);
                        qint64 read = file.read(buffer.data()->data(), remaining);
                        if (read < 0)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error reading from '%1' (%2)").arg(file.fileName()).arg(file.errorString()));
                            break;
                        }

                        qint64 send = Socket->write(buffer.data()->data(), read);

                        if (send != read)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending data (%1)").arg(Socket->errorString()));
                            break;
                        }

                        sent += read;
                    }
                    while (sent < size);

                    if (sent < size)
                        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to send all data for '%1'").arg(file.fileName()));
                }
#endif
            }
        }
        else
        {
            off64_t sent = 0;
            off64_t size = sendsize;
            off64_t offset = m_ranges.isEmpty() ? 0 : m_ranges.value(0).first;

#if defined(Q_OS_LINUX)
            if (size > sent)
            {
                // sendfile64 accesses the socket directly, bypassing Qt's cache, so we must flush first
                Socket->flush();

                do
                {
                    off64_t remaining = qMin(size - sent, (off64_t)READ_CHUNK_SIZE);
                    off64_t send = sendfile64(Socket->socketDescriptor(), file.handle(), &offset, remaining);

                    if (send < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            QThread::usleep(5000);
                            continue;
                        }

                        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending data (%1 - '%2')").arg(errno).arg(strerror(errno)));
                        break;
                    }
                    else
                    {
                        LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Sent %1 for %2").arg(send).arg(file.handle()));
                    }

                    sent += send;
                }
                while (sent < size);
            }
#elif defined(Q_OS_MAC)
            if (size > sent)
            {
                // sendfile accesses the socket directly, bypassing Qt's cache, so we must flush first
                Socket->flush();

                off_t bytessent  = 0;
                off_t off        = offset;

                do
                {
                    bytessent = qMin(size - sent, (qint64)READ_CHUNK_SIZE);;

                    if (sendfile(file.handle(), Socket->socketDescriptor(), off, &bytessent, nullptr, 0) < 0)
                    {
                        if (errno != EAGAIN)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending data (%1) %2").arg(errno).arg(strerror(errno)));
                            break;
                        }

                        QThread::usleep(5000);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Sent %1 for %2").arg(bytessent).arg(file.handle()));
                    }

                    sent += bytessent;
                    off  += bytessent;
                }
                while (sent < size);
            }
#else
            if (size > sent)
            {
                file.seek(offset);
                QScopedPointer<QByteArray> buffer(new QByteArray(READ_CHUNK_SIZE, 0));

                do
                {
                    qint64 remaining = qMin(size - sent, (qint64)READ_CHUNK_SIZE);
                    qint64 read = file.read(buffer.data()->data(), remaining);
                    if (read < 0)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error reading from '%1' (%2)").arg(file.fileName()).arg(file.errorString()));
                        break;
                    }

                    qint64 send = Socket->write(buffer.data()->data(), read);

                    if (send != read)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending data (%1)").arg(Socket->errorString()));
                        break;
                    }

                    sent += read;
                }
                while (sent < size);

                if (sent < size)
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to send all data for '%1'").arg(file.fileName()));
            }
#endif
        }
    }

    Socket->flush();

    if (m_connection == HTTPConnectionClose)
        Socket->disconnectFromHost();
}

void TorcHTTPRequest::Redirected(const QString &Redirected)
{
    m_redirectedTo = Redirected;
    m_responseStatus = HTTP_MovedPermanently;
    m_responseType   = HTTPResponseNone;
}

HTTPRequestType TorcHTTPRequest::RequestTypeFromString(const QString &Type)
{
    if (Type == QStringLiteral("GET"))     return HTTPGet;
    if (Type == QStringLiteral("HEAD"))    return HTTPHead;
    if (Type == QStringLiteral("POST"))    return HTTPPost;
    if (Type == QStringLiteral("PUT"))     return HTTPPut;
    if (Type == QStringLiteral("OPTIONS")) return HTTPOptions;
    if (Type == QStringLiteral("DELETE"))  return HTTPDelete;

    return HTTPUnknownType;
}

QString TorcHTTPRequest::RequestTypeToString(HTTPRequestType Type)
{
    switch (Type)
    {
        case HTTPHead:     return QStringLiteral("HEAD");
        case HTTPGet:      return QStringLiteral("GET");
        case HTTPPost:     return QStringLiteral("POST");
        case HTTPPut:      return QStringLiteral("PUT");
        case HTTPDelete:   return QStringLiteral("DELETE");
        case HTTPOptions:  return QStringLiteral("OPTIONS");
        case HTTPDisabled: return QStringLiteral("DISABLED"); // for completeness
        default:
            break;
    }

    return QStringLiteral("UNKNOWN");
}

HTTPProtocol TorcHTTPRequest::ProtocolFromString(const QString &Protocol)
{
    if (Protocol.startsWith(QStringLiteral("HTTP")))
    {
        if (Protocol.endsWith(QStringLiteral("1.1"))) return HTTPOneDotOne;
        if (Protocol.endsWith(QStringLiteral("1.0"))) return HTTPOneDotZero;
        if (Protocol.endsWith(QStringLiteral("0.9"))) return HTTPZeroDotNine;
    }

    return HTTPUnknownProtocol;
}

HTTPStatus TorcHTTPRequest::StatusFromString(const QString &Status)
{
    if (Status.startsWith(QStringLiteral("200"))) return HTTP_OK;
    if (Status.startsWith(QStringLiteral("101"))) return HTTP_SwitchingProtocols;
    if (Status.startsWith(QStringLiteral("206"))) return HTTP_PartialContent;
    if (Status.startsWith(QStringLiteral("301"))) return HTTP_MovedPermanently;
    if (Status.startsWith(QStringLiteral("304"))) return HTTP_NotModified;
    //if (Status.startsWith(QStringLiteral("400"))) return HTTP_BadRequest;
    if (Status.startsWith(QStringLiteral("401"))) return HTTP_Unauthorized;
    if (Status.startsWith(QStringLiteral("402"))) return HTTP_Forbidden;
    if (Status.startsWith(QStringLiteral("404"))) return HTTP_NotFound;
    if (Status.startsWith(QStringLiteral("405"))) return HTTP_MethodNotAllowed;
    if (Status.startsWith(QStringLiteral("416"))) return HTTP_RequestedRangeNotSatisfiable;
    if (Status.startsWith(QStringLiteral("429"))) return HTTP_TooManyRequests;
    if (Status.startsWith(QStringLiteral("500"))) return HTTP_InternalServerError;

    return HTTP_BadRequest;
}

QString TorcHTTPRequest::ProtocolToString(HTTPProtocol Protocol)
{
    switch (Protocol)
    {
        case HTTPOneDotOne:       return QStringLiteral("HTTP/1.1");
        case HTTPOneDotZero:      return QStringLiteral("HTTP/1.0");
        case HTTPZeroDotNine:     return QStringLiteral("HTTP/0.9");
        case HTTPUnknownProtocol: return QStringLiteral("Error");
    }

    return QStringLiteral("Error");
}

QString TorcHTTPRequest::StatusToString(HTTPStatus Status)
{
    switch (Status)
    {
        case HTTP_SwitchingProtocols:  return QStringLiteral("101 Switching Protocols");
        case HTTP_OK:                  return QStringLiteral("200 OK");
        case HTTP_PartialContent:      return QStringLiteral("206 Partial Content");
        case HTTP_MovedPermanently:    return QStringLiteral("301 Moved Permanently");
        case HTTP_NotModified:         return QStringLiteral("304 Not Modified");
        case HTTP_BadRequest:          return QStringLiteral("400 Bad Request");
        case HTTP_Unauthorized:        return QStringLiteral("401 Unauthorized");
        case HTTP_Forbidden:           return QStringLiteral("403 Forbidden");
        case HTTP_NotFound:            return QStringLiteral("404 Not Found");
        case HTTP_MethodNotAllowed:    return QStringLiteral("405 Method Not Allowed");
        case HTTP_RequestedRangeNotSatisfiable: return QStringLiteral("416 Requested Range Not Satisfiable");
        case HTTP_TooManyRequests:     return QStringLiteral("429 Too Many Requests");
        case HTTP_InternalServerError: return QStringLiteral("500 Internal Server Error");
        case HTTP_NotImplemented:      return QStringLiteral("501 Not Implemented");
        case HTTP_BadGateway:          return QStringLiteral("502 Bad Gateway");
        case HTTP_ServiceUnavailable:  return QStringLiteral("503 Service Unavailable");
        case HTTP_NetworkAuthenticationRequired: return QStringLiteral("511 Network Authentication Required");
    }

    return QStringLiteral("Error");
}

QString TorcHTTPRequest::ResponseTypeToString(HTTPResponseType Response)
{
    switch (Response)
    {
        case HTTPResponseNone:             return QStringLiteral("");
        case HTTPResponseXML:              return QStringLiteral("text/xml; charset=\"UTF-8\"");
        case HTTPResponseHTML:             return QStringLiteral("text/html; charset=\"UTF-8\"");
        case HTTPResponseJSON:             return QStringLiteral("application/json");
        case HTTPResponseJSONJavascript:   return QStringLiteral("text/javascript");
        case HTTPResponsePList:            return QStringLiteral("application/plist");
        case HTTPResponseBinaryPList:      return QStringLiteral("application/x-plist");
        case HTTPResponsePListApple:       return QStringLiteral("text/x-apple-plist+xml");
        case HTTPResponseBinaryPListApple: return QStringLiteral("application/x-apple-binary-plist");
        case HTTPResponsePlainText:        return QStringLiteral("text/plain");
        case HTTPResponseM3U8:             return QStringLiteral("application/x-mpegurl");
        case HTTPResponseM3U8Apple:        return QStringLiteral("application/vnd.apple.mpegurl");
        case HTTPResponseMPD:              return QStringLiteral("application/dash+xml");
        case HTTPResponseMPEGTS:           return QStringLiteral("video/mp2t");
        case HTTPResponseMP4:              return QStringLiteral("video/mp4");
        default: break;
    }

    return QStringLiteral("text/plain");
}

QString TorcHTTPRequest::AllowedToString(int Allowed)
{
    QStringList result;

    if (Allowed & HTTPHead)    result << QStringLiteral("HEAD");
    if (Allowed & HTTPGet)     result << QStringLiteral("GET");
    if (Allowed & HTTPPost)    result << QStringLiteral("POST");
    if (Allowed & HTTPPut)     result << QStringLiteral("PUT");
    if (Allowed & HTTPDelete)  result << QStringLiteral("DELETE");
    if (Allowed & HTTPOptions) result << QStringLiteral("OPTIONS");

    return result.join(QStringLiteral(", "));
}

QString TorcHTTPRequest::ConnectionToString(HTTPConnection Connection)
{
    switch (Connection)
    {
        case HTTPConnectionClose:     return QStringLiteral("close");
        case HTTPConnectionKeepAlive: return QStringLiteral("keep-alive");
        case HTTPConnectionUpgrade:   return QStringLiteral("Upgrade");
    }

    return QStringLiteral();
}

int TorcHTTPRequest::StringToAllowed(const QString &Allowed)
{
    int allowed = 0;

    if (Allowed.contains(QStringLiteral("HEAD"),    Qt::CaseInsensitive)) allowed += HTTPHead;
    if (Allowed.contains(QStringLiteral("GET"),     Qt::CaseInsensitive)) allowed += HTTPGet;
    if (Allowed.contains(QStringLiteral("POST"),    Qt::CaseInsensitive)) allowed += HTTPPost;
    if (Allowed.contains(QStringLiteral("PUT"),     Qt::CaseInsensitive)) allowed += HTTPPut;
    if (Allowed.contains(QStringLiteral("DELETE"),  Qt::CaseInsensitive)) allowed += HTTPDelete;
    if (Allowed.contains(QStringLiteral("OPTIONS"), Qt::CaseInsensitive)) allowed += HTTPOptions;
    if (Allowed.contains(QStringLiteral("AUTH"),    Qt::CaseInsensitive)) allowed += HTTPAuth;

    return allowed;
}

QVector<QPair<quint64,quint64> > TorcHTTPRequest::StringToRanges(const QString &Ranges, qint64 Size, qint64 &SizeToSend)
{
    qint64 tosend = 0;
    QVector<QPair<quint64,quint64> > results;
    if (Size < 1)
        return results;

    if (Ranges.contains(QStringLiteral("bytes"), Qt::CaseInsensitive))
    {
        QStringList newranges = Ranges.split('=');
        if (newranges.size() == 2)
        {
            QStringList rangelist = newranges[1].split(',', QString::SkipEmptyParts);

            foreach (const QString &range, rangelist)
            {
                QStringList parts = range.split('-');

                if (parts.size() != 2)
                    continue;

                qint64 start = 0;
                qint64 end = 0;
                bool ok = false;
                bool havefirst  = !parts[0].trimmed().isEmpty();
                bool havesecond = !parts[1].trimmed().isEmpty();

                if (havefirst && havesecond)
                {
                    start = parts[0].toULongLong(&ok);
                    if (ok)
                    {
                        // invalid per spec
                        if (start >= Size)
                            continue;

                        end = parts[1].toULongLong(&ok);

                        // invalid per spec
                        if (end < start)
                            continue;
                    }
                }
                else if (havefirst && !havesecond)
                {
                    start = parts[0].toULongLong(&ok);

                    // invalid per spec
                    if (ok && start >= Size)
                        continue;

                    end = Size - 1;
                }
                else if (!havefirst && havesecond)
                {
                    end = Size -1;
                    start = parts[1].toULongLong(&ok);
                    if (ok)
                    {
                        start = Size - start;

                        // invalid per spec
                        if (start >= Size)
                            continue;
                    }
                }

                if (ok)
                {
                    results << QPair<quint64,quint64>(start, end);
                    tosend += end - start + 1;
                }
            }
        }
    }

    if (results.isEmpty())
    {
        SizeToSend = 0;
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error parsing byte ranges ('%1')").arg(Ranges));
    }
    else
    {
        // rationalise overlapping range requests
        bool check = true;

        while (results.size() > 1 && check)
        {
            check = false;
            quint64 laststart = results[0].first;
            quint64 lastend   = results[0].second;

            for (int i = 1; i < results.size(); ++i)
            {
                quint64 thisstart = results[i].first;
                quint64 thisend   = results[i].second;

                if (thisstart > lastend)
                {
                    laststart = thisstart;
                    lastend   = thisend;
                    continue;
                }

                tosend -= (lastend - laststart + 1);
                tosend -= (thisend - thisstart + 1);
                results[i - 1].first  = qMin(laststart, thisstart);
                results[i - 1].second = qMax(lastend,   thisend);
                tosend += results[i - 1].second - results[i - 1].first + 1;
                results.removeAt(i);
                check = true;
                break;
            }
        }

        // set content size
        SizeToSend = tosend;
    }

    return results;
}

QString TorcHTTPRequest::RangeToString(QPair<quint64, quint64> Range, qint64 Size)
{
    return QStringLiteral("%1-%2/%3").arg(Range.first).arg(Range.second).arg(Size);
}

void TorcHTTPRequest::Serialise(const QVariant &Data, const QString &Type)
{
    TorcSerialiser *serialiser = TorcSerialiser::GetSerialiser(m_headers.value(QStringLiteral("Accept")));
    SetResponseType(serialiser->ResponseType());
    serialiser->Serialise(m_responseContent, Data, Type);
    SetResponseContent(m_responseContent);
    delete serialiser;
}

/*! \brief Return true if the resource is unmodified.
 *
 * The client must have supplied the 'If-Modified-Since' header and the request must have
 * last-modified caching enabled.
*/
bool TorcHTTPRequest::Unmodified(const QDateTime &LastModified)
{
    if ((m_cache & HTTPCacheLastModified) && m_headers.contains(QStringLiteral("If-Modified-Since")))
    {
        QDateTime since = QDateTime::fromString(m_headers.value(QStringLiteral("If-Modified-Since")), DateFormat);

        if (LastModified <= since)
        {
            SetStatus(HTTP_NotModified);
            SetResponseType(HTTPResponseNone);
            return true;
        }
    }

    return false;
}

/*! \brief Check whether the resource is equivalent to the last seen version.
 *
 * This method validates the ETag header, which must have been set locally and the client must
 * have sent the 'If-None-Match' header.
 *
 * \note ETag's are enclosed in parentheses. This code expects m_cacheTag to already be enclosed (i.e. the incoming
 * ETag is not stripped).
*/
bool TorcHTTPRequest::Unmodified(void)
{
    if ((m_cache & HTTPCacheETag) && !m_cacheTag.isEmpty() && m_headers.contains(QStringLiteral("If-None-Match")))
    {
        if (m_cacheTag == m_headers.value(QStringLiteral("If-None-Match")))
        {
            SetStatus(HTTP_NotModified);
            SetResponseType(HTTPResponseNone);
            return true;
        }
    }

    return false;
}

void TorcHTTPRequest::Authorise(HTTPAuthorisation Authorisation)
{
    m_authorised = Authorisation;
}

HTTPAuthorisation TorcHTTPRequest::IsAuthorised(void) const
{
    return m_authorised;
}
