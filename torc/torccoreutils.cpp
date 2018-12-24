/* TorcCoreUtils
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
#include <QThread>
#include <QStringList>

// Std
#include <time.h>
#include <errno.h>

// Torc
#include "torccompat.h"
#include "torclogging.h"
#include "torccoreutils.h"

// zlib
#ifdef USING_ZLIB
#include "zlib.h"
#endif

/// \brief Parse a QDateTime from the given QString
QDateTime TorcCoreUtils::DateTimeFromString(const QString &String)
{
    if (String.isEmpty())
        return QDateTime();

    if (!String.contains('-') && String.length() == 14)
    {
        // must be in yyyyMMddhhmmss format
        return QDateTime::fromString(String, QStringLiteral("yyyyMMddhhmmss"));
    }

    return QDateTime::fromString(String, Qt::ISODate);
}

/*! \brief Get the current system clock time in microseconds.
 *
 * If microsecond clock accuracy is not available, it is inferred from the best
 * resolution available.
*/
quint64 TorcCoreUtils::GetMicrosecondCount(void)
{
#ifdef Q_OS_WIN
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER ticks;
    if (QueryPerformanceFrequency(&ticksPerSecond))
    {
        QueryPerformanceCounter(&ticks);
        return (quint64)(((qreal)ticks.QuadPart / ticksPerSecond.QuadPart) * 1000000);
    }
    return GetTickCount() * 1000;
#elif defined(Q_OS_MAC)
    struct timeval timenow;
    gettimeofday(&timenow, nullptr);
    return (timenow.tv_sec * 1000000) + timenow.tv_usec;
#else
    timespec timenow;
    clock_gettime(CLOCK_MONOTONIC, &timenow);
    return (timenow.tv_sec * 1000000) + ((timenow.tv_nsec + 500) / 1000);
#endif
}

/*! \brief A handler routine for Qt messages.
 *
 * This ensures Qt warnings are included in non-console logs.
*/
void TorcCoreUtils::QtMessage(QtMsgType Type, const QMessageLogContext &Context, const QString &Message)
{
    const char *function = Context.function ? Context.function : __FUNCTION__;
    const char *file     = Context.file ? Context.file : __FILE__;
    int         line     = Context.line ? Context.line : __LINE__;

    int level = LOG_UNKNOWN;
    switch (Type)
    {
        case QtFatalMsg: level = LOG_CRIT; break;
        case QtDebugMsg: level = LOG_INFO; break;
        default: level = LOG_ERR;
    }

    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, level))
    {
        PrintLogLine(VB_GENERAL, (LogLevel)level,
                     file, line, function,
                     Message.toLocal8Bit().constData());
    }

    if (LOG_CRIT == level)
    {
        QThread::msleep(100);
        abort();
    }
}

///\brief Return true if zlib support is available.
bool TorcCoreUtils::HasZlib(void)
{
#ifdef USING_ZLIB
    return true;
#else
    return false;
#endif
}

/*! \brief Compress the supplied data using GZip.
 *
 * The returned data is suitable for sending as part of an HTTP response when the requestor accepts
 * gzip compression.
*/
QByteArray TorcCoreUtils::GZipCompress(QByteArray &Source)
{
    QByteArray result;

#ifndef USING_ZLIB
    (void) Source;
    return result;
#else
    // this shouldn't happen
    if (Source.size() < 0)
        return result;

    // initialise zlib (see zlib usage examples)
    static const int chunksize = 16384;
    char outputbuffer[chunksize];

    z_stream stream;
    stream.zalloc   = nullptr;
    stream.zfree    = nullptr;
    stream.opaque   = nullptr;
    stream.avail_in = Source.size();
    stream.next_in  = (Bytef*)Source.data();

    if (Z_OK != deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to setup zlip decompression"));
        return result;
    }

    do
    {
        stream.avail_out = chunksize;
        stream.next_out  = (Bytef*)outputbuffer;

        int error = deflate(&stream, Z_FINISH);

        if (Z_NEED_DICT == error || error < Z_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to compress data"));
            deflateEnd(&stream);
            return result;
        }

        result.append(outputbuffer, chunksize - stream.avail_out);
    } while (stream.avail_out == 0);

    deflateEnd(&stream);
    return result;
#endif
}

/*! \brief Compress the given file using GZip.
 *
 * The returned data is suitable for sending as part of an HTTP response when the requestor accepts
 * gzip compression.
*/
QByteArray TorcCoreUtils::GZipCompressFile(QFile &Source)
{
    QByteArray result;

#ifndef USING_ZLIB
    (void) Source;
    return result;
#else
    // this shouldn't happen
    if (Source.size() < 0)
        return result;

    // initialise zlib (see zlib usage examples)
    static const int chunksize = 32768;
    char inputbuffer[chunksize];
    char outputbuffer[chunksize];

    z_stream stream;
    stream.zalloc   = nullptr;
    stream.zfree    = nullptr;
    stream.opaque   = nullptr;

    if (Z_OK != deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to setup zlip decompression"));
        return result;
    }

    // this should have been checked already
    if (!Source.isOpen())
        if (!Source.open(QIODevice::ReadOnly))
            return result;

    while (Source.bytesAvailable() > 0)
    {
        stream.avail_out = chunksize;
        stream.next_out  = (Bytef*)outputbuffer;

        qint64 read = Source.read(inputbuffer, qMin(Source.bytesAvailable(), (qint64)chunksize));

        if (read > 0)
        {
            stream.avail_in = read;
            stream.next_in  = (Bytef*)inputbuffer;

            int error = deflate(&stream, Source.bytesAvailable() ? Z_SYNC_FLUSH : Z_FINISH);

            if (error == Z_OK || error == Z_STREAM_END)
            {
                result.append(outputbuffer, chunksize - stream.avail_out);
                continue;
            }
        }

        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to compress file"));
        deflateEnd(&stream);
        return result;
    }

    deflateEnd(&stream);
    return result;
#endif
}
