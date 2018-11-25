/* Class TorcCameraOutput
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

#define AKAMAI_TIME_ISO QString("http://time.akamai.com/?iso")

// Qt
#include <QDir>

// Torc
#include "torclogging.h"
#include "torcmime.h"
#include "torcdirectories.h"
#include "torcnetworkrequest.h"
#include "torcoutputs.h"
#include "torccamerathread.h"
#include "torccameraoutput.h"

TorcCameraOutput::TorcCameraOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details,
                                   QObject *Output, const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcOutput(Type, Value, ModelId, Details, Output, MetaObject, Blacklist + "," + "CameraErrored"),
    m_thread(NULL),
    m_threadLock(QReadWriteLock::Recursive),
    m_params(Details)
{
}

TorcCameraOutput::~TorcCameraOutput()
{
}

TorcCameraParams& TorcCameraOutput::GetParams(void)
{
    return m_params;
}

void TorcCameraOutput::SetParams(TorcCameraParams &Params)
{
    m_params = Params;
}

TorcCameraStillsOutput::TorcCameraStillsOutput(const QString &ModelId, const QVariantMap &Details)
  : TorcCameraOutput(TorcOutput::Camera, 0.0, ModelId, Details, this, TorcCameraStillsOutput::staticMetaObject,
               "StillReady"),
    stillsList(),
    m_stillsList(),
    m_stillsDirectory()
{
    m_stillsDirectory = GetTorcContentDir() + ModelId + "/";
    m_params.m_contentDir = m_stillsDirectory;

    // populate m_stillsList - camera has not started yet, so should be static
    QDir stillsdir(m_stillsDirectory);
    if (!stillsdir.exists())
        stillsdir.mkpath(m_stillsDirectory);
    QStringList namefilters;
    QStringList imagefilters = TorcMime::ExtensionsForType("image");
    foreach (QString image, imagefilters)
        { namefilters << QString("*." + image); }
    QFileInfoList stills = stillsdir.entryInfoList(namefilters, QDir::NoDotAndDotDot | QDir::Files | QDir::Readable, QDir::Name);
    foreach (QFileInfo file, stills)
        m_stillsList.append(file.fileName());
}

TorcCameraStillsOutput::~TorcCameraStillsOutput()
{
    Stop();
}

void TorcCameraStillsOutput::Stop(void)
{
    m_threadLock.lockForWrite();
    if (m_thread)
    {
        disconnect(this, &TorcCameraStillsOutput::ValueChanged, this, &TorcCameraStillsOutput::TakeStills);
        TorcCameraThread::CreateOrDestroy(m_thread, modelId);
        m_thread = NULL;
    }
    m_threadLock.unlock();
}

void TorcCameraStillsOutput::Start(void)
{
    Stop();

    m_threadLock.lockForWrite();
    TorcCameraThread::CreateOrDestroy(m_thread, modelId, m_params);
    if (m_thread)
    {
        m_thread->SetStillsParent(this);
        m_thread->start();
        // listen for requests
        connect(this, &TorcCameraStillsOutput::ValueChanged, this, &TorcCameraStillsOutput::TakeStills);
    }
    m_threadLock.unlock();
}

QStringList TorcCameraStillsOutput::GetStillsList(void)
{
    QReadLocker locker(&m_threadLock);
    return m_stillsList;
}

TorcOutput::Type TorcCameraStillsOutput::GetType(void)
{
    return TorcOutput::Camera;
}

void TorcCameraStillsOutput::CameraErrored(bool Errored)
{
    SetValid(!Errored);
    if (Errored)
    {
        LOG(VB_GENERAL, LOG_ERR, "Camera reported error");
        Stop();
    }
}

void TorcCameraStillsOutput::StillReady(const QString File)
{
    QWriteLocker locker(&m_threadLock);
    if (m_stillsList.contains(File))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Still '%1' is duplicate - ignoring").arg(File));
    }
    else
    {
        m_stillsList.append(File);
        emit StillsListChanged(m_stillsList);
    }
}

TorcCameraVideoOutput::TorcCameraVideoOutput(const QString &ModelId, const QVariantMap &Details)
  : TorcCameraOutput(TorcOutput::Camera, 0.0, ModelId, Details, this, TorcCameraVideoOutput::staticMetaObject,
               "WritingStarted,WritingStopped,SegmentRemoved,InitSegmentReady,SegmentReady"),
    m_segments(),
    m_segmentLock(QReadWriteLock::Recursive),
    m_cameraStartTime(),
    m_networkTimeAbort(0),
    m_networkTimeRequest(NULL)
{
    // keep time checks in this thread
    connect(this, SIGNAL(CheckTime()), this, SLOT(TimeCheck()));
    // and check initially
    emit CheckTime();
}

TorcCameraVideoOutput::~TorcCameraVideoOutput()
{
    Stop();

    m_networkTimeAbort = 1;

    if (m_networkTimeRequest)
    {
        TorcNetwork::Cancel(m_networkTimeRequest);
        m_networkTimeRequest->DownRef();
    }
}

void TorcCameraVideoOutput::TimeCheck(void)
{
    if (m_networkTimeRequest)
    {
        LOG(VB_GENERAL, LOG_INFO, "Time check in progress - not resending");
        return;
    }

    QUrl url(AKAMAI_TIME_ISO);
    QNetworkRequest request(url);
    m_networkTimeRequest = new TorcNetworkRequest(request, QNetworkAccessManager::GetOperation, 0, &m_networkTimeAbort);
    TorcNetwork::GetAsynchronous(m_networkTimeRequest, this);
}

void TorcCameraVideoOutput::RequestReady(TorcNetworkRequest *Request)
{
    if (Request && m_networkTimeRequest && (Request == m_networkTimeRequest))
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Network time  : %1").arg(Request->GetBuffer().constData()));
        m_networkTimeRequest->DownRef();
        m_networkTimeRequest = NULL;
    }
}

TorcOutput::Type TorcCameraVideoOutput::GetType(void)
{
    return TorcOutput::Camera;
}

QString TorcCameraVideoOutput::GetPresentationURL(void)
{
    return QString("%1%2").arg(m_signature).arg(VIDEO_PAGE);
}

void TorcCameraVideoOutput::Start(void)
{
    Stop();

    m_threadLock.lockForWrite();
    TorcCameraThread::CreateOrDestroy(m_thread, modelId, m_params);
    if (m_thread)
    {
        m_thread->SetVideoParent(this);
        m_thread->start();
        connect(this, SIGNAL(ValueChanged(double)), this, SIGNAL(StreamVideo(bool)));
    }
    m_threadLock.unlock();
}

/// The 'init' segment is ready and available.
void TorcCameraVideoOutput::InitSegmentReady(void)
{
    QReadLocker locker(&m_threadLock);
    if (m_thread)
    {
        m_params = m_thread->GetParams();
        LOG(VB_GENERAL, LOG_INFO, QString("Initial segment ready - codec '%1'").arg(m_params.m_videoCodec));
        // allow remote clients to start reading once the init segment is saved and hence codec information
        // is available
        m_cameraStartTime = QDateTime::currentDateTimeUtc();
    }
}

/// The camera has started to record video.
void TorcCameraVideoOutput::WritingStarted(void)
{
    // we actually deem video output to be valid once the init segment has been received
    // (which is signalled separately)
    LOG(VB_GENERAL, LOG_INFO, "Camera started");
}

void TorcCameraVideoOutput::Stop(void)
{
    m_segmentLock.lockForWrite();
    m_segments.clear();
    m_segmentLock.unlock();

    m_threadLock.lockForWrite();
    if (m_thread)
    {
        disconnect(this, SIGNAL(ValueChanged(double)), this, SIGNAL(StreamVideo(bool)));
        TorcCameraThread::CreateOrDestroy(m_thread, modelId);
        m_thread = NULL;
    }
    m_threadLock.unlock();
}

/// The camera is no longer recording any video.
void TorcCameraVideoOutput::WritingStopped(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Camera stopped");
    m_threadLock.lockForWrite();
    m_cameraStartTime = QDateTime();
    m_threadLock.unlock();
}

void TorcCameraVideoOutput::CameraErrored(bool Errored)
{
    SetValid(!Errored);
    if (Errored)
    {
        LOG(VB_GENERAL, LOG_ERR, "Camera reported error");
        Stop();
    }
}

void TorcCameraVideoOutput::SegmentReady(int Segment)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Segment %1 ready").arg(Segment));

    QWriteLocker locker(&m_segmentLock);
    if (!m_segments.contains(Segment))
    {
        if (!m_segments.isEmpty() && m_segments.head() >= Segment)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Segment %1 is not greater than head (%2)").arg(Segment).arg(m_segments.head()));
        }
        else
        {
            m_segments.enqueue(Segment);
            // track drift - m_cameraStartTime points to the beginning of the first segment
            qint64 drift = m_cameraStartTime.addMSecs((m_segments.last() + 1) * (VIDEO_SEGMENT_TARGET * 1000)).msecsTo(QDateTime::currentDateTime());
            LOG(VB_GENERAL, LOG_INFO, QString("Drift %1ms (%2) first %3 last %4")
                .arg(drift).arg(drift > 0 ? "behind" : "ahead").arg(m_segments.first()).arg(m_segments.last()));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Already have a segment #%1").arg(Segment));
    }
}

void TorcCameraVideoOutput::SegmentRemoved(int Segment)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Segment %1 removed").arg(Segment));

    QWriteLocker locker(&m_segmentLock);

    if (m_segments.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot remove segment - segments list is empty");
    }
    else
    {
        if (m_segments.first() != Segment)
            LOG(VB_GENERAL, LOG_ERR, QString("Segment %1 is not at tail of queue").arg(Segment));
        else
            m_segments.dequeue();
    }
}

void TorcCameraVideoOutput::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
{
    (void)PeerAddress;
    (void)PeerPort;
    (void)LocalAddress;
    (void)LocalPort;
    (void)Request;

    // treat camera feeds as secure - or maybe not - many players just don't seem to expect authorisation
    //if (!MethodIsAuthorised(Request, HTTPAuth))
    //    return;

    // cameraStartTime is set when the camera thread has started (and set to invalid when stopped)
    m_threadLock.lockForRead();
    bool valid = m_cameraStartTime.isValid();
    m_threadLock.unlock();
    if (!valid)
        return;

    Request.SetAllowCORS(true); // needed for a number of browser players.
    QString method = Request.GetMethod();
    HTTPRequestType type = Request.GetHTTPRequestType();

    bool hlsmaster   = method.compare(HLS_PLAYLIST_MAST) == 0;
    bool hlsplaylist = method.compare(HLS_PLAYLIST) == 0;
    bool player      = method.compare(VIDEO_PAGE) == 0;
    bool dash        = method.compare(DASH_PLAYLIST) == 0;
    bool segment     = method.startsWith("segment") && method.endsWith(".m4s");
    bool init        = method.startsWith("init") && method.endsWith(".mp4");

    if (!(hlsplaylist || player || hlsmaster || dash || segment || init))
    {
        Request.SetStatus(HTTP_NotFound);
        Request.SetResponseType(HTTPResponseDefault);
        return;
    }

    if (type == HTTPOptions)
    {
        HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        if (segment || init)
            Request.SetResponseType(HTTPResponseMP4);
        else if (hlsmaster || hlsplaylist)
            Request.SetResponseType(HTTPResponseM3U8Apple);
        else if (player)
            Request.SetResponseType(HTTPResponseHTML);
        else
            Request.SetResponseType(HTTPResponseMPD);
        return;
    }

    if (type != HTTPGet && type != HTTPHead)
    {
        Request.SetStatus(HTTP_BadRequest);
        Request.SetResponseType(HTTPResponseDefault);
        return;
    }

    QByteArray result;

    if (hlsmaster)
    {
        LOG(VB_GENERAL, LOG_DEBUG, "Sending master HLS playlist");
        result = GetMasterPlaylist();
        Request.SetAllowGZip(true);
        Request.SetResponseType(HTTPResponseM3U8Apple);
    }
    else if (hlsplaylist)
    {
        LOG(VB_GENERAL, LOG_DEBUG, "Sending HLS playlist");
        result = GetHLSPlaylist();
        Request.SetAllowGZip(true);
        Request.SetResponseType(HTTPResponseM3U8Apple);
    }
    else if (player)
    {
        LOG(VB_GENERAL, LOG_DEBUG, "Sending video page");
        result = GetPlayerPage();
        Request.SetAllowGZip(true);
        Request.SetResponseType(HTTPResponseHTML);
    }
    else if (dash)
    {
        LOG(VB_GENERAL, LOG_DEBUG, "Sending DASH playlist");
        result = GetDashPlaylist();
        Request.SetAllowGZip(true);
        Request.SetResponseType(HTTPResponseMPD);
    }
    else if (segment)
    {
        QString number = method.mid(7);
        number.chop(4);
        bool ok = false;
        int num = number.toInt(&ok);
        if (ok)
        {
            QReadLocker locker(&m_threadLock);
            LOG(VB_GENERAL, LOG_DEBUG, QString("Segment %1 requested").arg(num));
            result = m_thread->GetSegment(num);
            if (!result.isEmpty())
            {
                Request.SetAllowGZip(false);
                Request.SetResponseType(HTTPResponseMP4);
            }
            else
            {
                QReadLocker locker(&m_segmentLock);
                if (m_segments.isEmpty())
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("No segments - %1 requested").arg(num));
                }
                else
                {
                    m_threadLock.lockForRead();
                    QDateTime start = m_cameraStartTime;
                    m_threadLock.unlock();
                    LOG(VB_GENERAL, LOG_WARNING, QString("Segment %1 not found - we have %2-%3")
                        .arg(num).arg(m_segments.first()).arg(m_segments.last()));
                    LOG(VB_GENERAL, LOG_WARNING, QString("Our start time: %1").arg(start.toString(Qt::ISODate)));
                    LOG(VB_GENERAL, LOG_WARNING, QString("Start+request : %1").arg(start.addSecs(num *2).toString(Qt::ISODate)));
                    LOG(VB_GENERAL, LOG_WARNING, QString("Start+first   : %1").arg(start.addSecs(m_segments.first() * 2).toString(Qt::ISODate)));
                    LOG(VB_GENERAL, LOG_WARNING, QString("Start+last    : %1").arg(start.addSecs(m_segments.last() * 2).toString(Qt::ISODate)));
                    LOG(VB_GENERAL, LOG_WARNING, QString("System time   : %1").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)));
                    emit CheckTime();
                }

            }
        }
        else
        {
            Request.SetStatus(HTTP_BadRequest);
            Request.SetResponseType(HTTPResponseDefault);
            return;
        }
    }
    else if (init)
    {
        QReadLocker locker(&m_threadLock);
        LOG(VB_GENERAL, LOG_DEBUG, "Init segment requested");
        result = m_thread->GetInitSegment();
        if (!result.isEmpty())
        {
            Request.SetAllowGZip(false);
            Request.SetResponseType(HTTPResponseMP4);
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, "Init segment not found");
        }
    }

    if (!result.isEmpty())
    {
        Request.SetResponseContent(result);
        Request.SetStatus(HTTP_OK);
    }
    else
    {
        Request.SetStatus(HTTP_NotFound);
        Request.SetResponseType(HTTPResponseDefault);
    }
}

QByteArray TorcCameraVideoOutput::GetPlayerPage(void)
{
    static const QString player("<html>\r\n"
                                "  <script src=\"/js/vendor/dash-2.9.0.all.min.js\"></script>\r\n"
                                "  <body>\r\n"
                                "    <div>\r\n"
                                "      <video data-dashjs-player autoplay src=\"%1\" controls></video>\r\n"
                                "    </div>\r\n"
                                "  </body>\r\n"
                                "</html>\r\n");

    return QByteArray(player.arg(DASH_PLAYLIST).toLocal8Bit());
}

QByteArray TorcCameraVideoOutput::GetMasterPlaylist(void)
{
    static const QString playlist("#EXTM3U\r\n"
                                  "#EXT-X-VERSION:4\r\n"
                                  "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%1,RESOLUTION=%2x%3,CODECS=\"%5\"\r\n"
                                  "%4\r\n");

    return QByteArray(playlist.arg(m_params.m_bitrate).arg(m_params.m_width).arg(m_params.m_height)
                                  .arg(HLS_PLAYLIST).arg(m_params.m_videoCodec)/*.arg(AUDIO_CODEC_ISO)*/.toLocal8Bit());
}

QByteArray TorcCameraVideoOutput::GetHLSPlaylist(void)
{
    static const QString playlist("#EXTM3U\r\n"
                                  "#EXT-X-VERSION:4\r\n"
                                  "#EXT-X-TARGETDURATION:%1\r\n"
                                  "#EXT-X-MEDIA-SEQUENCE:%2\r\n"
                                  "#EXT-X-MAP:URI=\"initSegment.mp4\"\r\n");

    QString duration = QString::number(m_params.m_segmentLength / (float) m_params.m_frameRate, 'f', 2);
    m_segmentLock.lockForRead();
    QString result = playlist.arg(duration).arg(m_segments.first());
    foreach (int segment, m_segments)
    {
        result += QString("#EXTINF:%1,\r\n").arg(duration);
        result += QString("segment%1.m4s\r\n").arg(segment);
    }
    m_segmentLock.unlock();
    return QByteArray(result.toLocal8Bit());
}

QByteArray TorcCameraVideoOutput::GetDashPlaylist(void)
{
    static const QString dash(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n"
        "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" profiles=\"urn:mpeg:dash:profile:isoff-live:2011\""
        " type=\"dynamic\" availabilityStartTime=\"%1\" minimumUpdatePeriod=\"PT%2S\""
        " publishTime=\"%1\" timeShiftBufferDepth=\"PT%3S\" minBufferTime=\"PT%4S\">\r\n"
        "  <Period id=\"0\" start=\"PT0S\">\r\n"
        "    <AdaptationSet contentType=\"video\" segmentAlignment=\"true\" startWithSAP=\"1\">\r\n"
        "      <Representation id=\"default\" mimeType=\"%5\" width=\"%6\" height=\"%7\" bandwidth=\"%8\" codecs=\"%9\" frameRate=\"%10\">\r\n"
        "        <SegmentTemplate duration=\"%11\" timescale=\"%12\" initialization=\"initSegment.mp4\" media=\"segment$Number$.m4s\" startNumber=\"0\" />\r\n"
        "      </Representation>\r\n"
        "    </AdaptationSet>\r\n"
        "  </Period>\r\n"
        "</MPD>\r\n");

    double duration = m_params.m_segmentLength / (float)m_params.m_frameRate;
    m_threadLock.lockForRead();
    QString start = m_cameraStartTime.toString(Qt::ISODate);
    m_threadLock.unlock();
    return QByteArray(dash.arg(start)
                      .arg(QString::number(duration * 5, 'f', 2))
                      .arg(QString::number(duration * 4, 'f', 2))
                      .arg(QString::number(duration * 2, 'f', 2))
                      .arg(TorcHTTPRequest::ResponseTypeToString(HTTPResponseMP4/*HTTPResponseMPEGTS*/))
                      .arg(m_params.m_width)
                      .arg(m_params.m_height)
                      .arg(m_params.m_bitrate)
                      .arg(QString("%1").arg(m_params.m_videoCodec))
                      //.arg(QString("%1,%2").arg(VIDEO_CODEC_ISO).arg(AUDIO_CODEC_ISO))
                      .arg(QString("%1").arg(m_params.m_frameRate))
                      .arg(duration * m_params.m_timebase)
                      .arg(m_params.m_timebase).toLocal8Bit());
}

TorcCameraOutputs* TorcCameraOutputs::gCameraOutputs = new TorcCameraOutputs();

TorcCameraOutputs::TorcCameraOutputs()
  : TorcDeviceHandler(),
    m_cameras()
{
}

void TorcCameraOutputs::Create(const QVariantMap &Details)
{
    QMutexLocker locker(&m_lock);

    QVariantMap::const_iterator ii = Details.constBegin();
    for ( ; ii != Details.constEnd(); ++ii)
    {
        if (ii.key() != "outputs")
            continue;

        QVariantMap outputs = ii.value().toMap();
        QVariantMap::const_iterator i = outputs.constBegin();
        for ( ; i != outputs.constEnd(); ++i)
        {
            if (i.key() != "cameras")
                continue;

            QVariantMap cameras = i.value().toMap();
            QVariantMap::iterator it = cameras.begin();
            for ( ; it != cameras.end(); ++it)
            {
                QVariantMap types = it.value().toMap();
                QVariantMap::iterator it2 = types.begin();
                for ( ; it2 != types.end(); ++it2)
                {
                    bool video  = false;
                    bool stills = false;
                    QVariantMap camera;
                    if (it2.key() == "video")
                    {
                        video = true;
                        camera = it2.value().toMap();
                    }
                    else if (it2.key() == "stills")
                    {
                        stills = true;
                        camera = it2.value().toMap();
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Unknown camera interface '%1'").arg(it2.key()));
                        continue;
                    }

                    if (!camera.contains("name"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' has no name").arg(it.key()));
                        continue;
                    }
                    if (!camera.contains("width"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' does not specify width").arg(camera.value("name").toString()));
                        continue;
                    }
                    if (!camera.contains("height"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' does not specify height").arg(camera.value("name").toString()));
                        continue;
                    }
                    bool bitrate   = camera.contains("bitrate");
                    bool framerate = camera.contains("framerate");

                    if (video && !bitrate)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Camera video interface '%1' does not specify bitrate").arg(camera.value("name").toString()));
                        continue;
                    }

                    if (video && !framerate)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Camera video interface '%1' does not specify framerate").arg(camera.value("name").toString()));
                        continue;
                    }

                    // NB TorcCameraFactory checks that the underlying class can handle the specified camera
                    // which ensures the TorcCameraOutput instance will be able to create the camera.
                    TorcCameraOutput *newcamera = NULL;
                    TorcCameraFactory* factory = TorcCameraFactory::GetTorcCameraFactory();
                    TorcCameraParams params(Details);
                    for ( ; factory; factory = factory->NextFactory())
                    {
                        if (factory->CanHandle(it.key(), params))
                        {
                            if (video)
                                newcamera = new TorcCameraVideoOutput(it.key(), camera);
                            if (stills)
                                newcamera = new TorcCameraStillsOutput(it.key(), camera);

                            if (newcamera)
                            {
                                m_cameras.insertMulti(it.key(), newcamera);
                                LOG(VB_GENERAL, LOG_INFO, QString("New '%1' camera '%2'").arg(it.key()).arg(newcamera->GetUniqueId()));
                                break;
                            }
                        }
                    }
                    if (NULL == newcamera)
                        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to find handler for camera '%1'").arg(it.key()));
                }
            }
        }
    }

    // we now need to analyse our list of cameras and combine the parameters for output devices that
    // reference the same camera device
    QList<QString> keys = m_cameras.uniqueKeys();
    foreach (QString key, keys)
    {
        QList<TorcCameraOutput*> outputs = m_cameras.values(key);
        TorcCameraParams params;
        foreach (TorcCameraOutput* output, outputs)
            params = params.Combine(output->GetParams());
        foreach (TorcCameraOutput* output, outputs)
            output->SetParams(params);
    }
}

void TorcCameraOutputs::Destroy(void)
{
    QMutexLocker locker(&m_lock);
    QHash<QString, TorcCameraOutput*>::iterator it = m_cameras.begin();
    for ( ; it != m_cameras.end(); ++it)
    {
        TorcOutputs::gOutputs->RemoveOutput(it.value());
        it.value()->DownRef();
    }
    m_cameras.clear();
}
