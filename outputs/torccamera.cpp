/* Class TorcCamera
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
#include "torclogging.h"
#include "torcoutputs.h"
#include "torccamera.h"

#define BLACKLIST QString("StartCamera,StopCamera,WritingStarted,WritingStopped")

TorcCameraParams::TorcCameraParams(void)
  : m_valid(false),
    m_width(0),
    m_height(0),
    m_stride(0),
    m_sliceHeight(0),
    m_frameRate(1),
    m_bitrate(0),
    m_timebase(VIDEO_TIMEBASE),
    m_segmentLength(0),
    m_gopSize(0),
    m_model(),
    m_videoCodec()
{
}

TorcCameraParams::TorcCameraParams(const QVariantMap &Details)
  : m_valid(false),
    m_width(0),
    m_height(0),
    m_stride(0),
    m_sliceHeight(0),
    m_frameRate(1),
    m_bitrate(0),
    m_timebase(VIDEO_TIMEBASE),
    m_segmentLength(0),
    m_gopSize(0),
    m_model(),
    m_videoCodec()
{
    if (!Details.contains("width")     || !Details.contains("height")  ||
        !Details.contains("framerate") || !Details.contains("bitrate") ||
        !Details.contains("model"))
    {
        return;
    }

    m_width     = Details.value("width").toInt();
    m_height    = Details.value("height").toInt();
    m_frameRate = Details.value("framerate").toInt();
    m_bitrate   = Details.value("bitrate").toInt();
    m_model     = Details.value("model").toString();

    // N.B. pitch and slice height may be further constrained in certain encoders and may be further adjusted
    // most h264 streams will expect 16 pixel aligned video so round up
    int pitch = (m_width + 15) & ~15;
    if (pitch != m_width)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Rounding video width up to %1 from %2").arg(pitch).arg(m_width));
        m_width = pitch;
    }

    // and 8 pixel macroblock height
    int height = (m_height + 7) & ~7;
    if (height != m_height)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Rounding video height up to %1 from %2").arg(height).arg(m_height));
        m_height = height;
    }

    m_stride      = m_width;
    m_sliceHeight = m_height;

    bool forcemin = m_width < VIDEO_WIDTH_MIN || m_height < VIDEO_HEIGHT_MIN;
    bool forcemax = m_width > VIDEO_WIDTH_MAX || m_height > VIDEO_HEIGHT_MAX;
    if (forcemin)
    {
        m_width  = VIDEO_WIDTH_MIN;
        m_height = VIDEO_HEIGHT_MIN;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video too small - forcing output to %1x%2").arg(m_width).arg(m_height));
    }
    else if (forcemax)
    {
        m_width  = VIDEO_WIDTH_MAX;
        m_height = VIDEO_HEIGHT_MAX;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video too large - forcing output to %1x%2").arg(m_width).arg(m_height));
    }

    if (m_frameRate < VIDEO_FRAMERATE_MIN)
    {
        m_frameRate = VIDEO_FRAMERATE_MIN;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video framerate too low - forcing to %1").arg(m_frameRate));
    }
    else if (m_frameRate > VIDEO_FRAMERATE_MAX)
    {
        m_frameRate = VIDEO_FRAMERATE_MAX;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video framerate too high - forcing to %1").arg(m_frameRate));
    }

    if (m_bitrate < VIDEO_BITRATE_MIN)
    {
        m_bitrate = VIDEO_BITRATE_MIN;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video bitrate too low - forcing to %1").arg(m_bitrate));
    }
    else if (m_bitrate > VIDEO_BITRATE_MAX)
    {
        m_bitrate = VIDEO_BITRATE_MAX;
        LOG(VB_GENERAL, LOG_WARNING, QString("Video bitrate too high - forcing to %1").arg(m_bitrate));
    }

    m_segmentLength = m_frameRate * VIDEO_SEGMENT_TARGET;
    m_gopSize       = m_frameRate * VIDEO_GOPDURA_TARGET;

    LOG(VB_GENERAL, LOG_INFO, QString("Segment length: %1frames %2seconds").arg(m_segmentLength).arg(m_segmentLength / m_frameRate));
    LOG(VB_GENERAL, LOG_INFO, QString("GOP     length: %1frames %2seconds").arg(m_gopSize).arg(m_gopSize / m_frameRate));
    LOG(VB_GENERAL, LOG_INFO, QString("Camera video  : %1x%2@%3fps bitrate %4").arg(m_width).arg(m_height).arg(m_frameRate).arg(m_bitrate));

    m_valid = true;
}

TorcCameraParams::TorcCameraParams(const TorcCameraParams &Other)
  : m_valid(Other.m_valid),
    m_width(Other.m_width),
    m_height(Other.m_height),
    m_stride(Other.m_stride),
    m_sliceHeight(Other.m_sliceHeight),
    m_frameRate(Other.m_frameRate),
    m_bitrate(Other.m_bitrate),
    m_timebase(Other.m_timebase),
    m_segmentLength(Other.m_segmentLength),
    m_gopSize(Other.m_gopSize),
    m_model(Other.m_model),
    m_videoCodec(Other.m_videoCodec)
{
}

TorcCameraParams& TorcCameraParams::operator =(const TorcCameraParams &Other)
{
    if (&Other != this)
    {
        this->m_valid = Other.m_valid;
        this->m_width = Other.m_width;
        this->m_height = Other.m_height;
        this->m_stride = Other.m_stride;
        this->m_sliceHeight = Other.m_sliceHeight;
        this->m_frameRate   = Other.m_frameRate;
        this->m_bitrate     = Other.m_bitrate;
        this->m_timebase    = Other.m_timebase;
        this->m_segmentLength = Other.m_segmentLength;
        this->m_gopSize       = Other.m_gopSize;
        this->m_model         = Other.m_model;
        this->m_videoCodec    = Other.m_videoCodec;
    }
    return *this;
}

TorcCameraDevice::TorcCameraDevice(const TorcCameraParams &Params)
  : m_params(Params),
    m_ringBuffer(NULL)
{
}

TorcCameraDevice::~TorcCameraDevice()
{
    if (m_ringBuffer)
    {
        delete m_ringBuffer;
        m_ringBuffer = NULL;
    }
}

QByteArray TorcCameraDevice::GetSegment(int Segment)
{
    if (m_ringBuffer)
        return m_ringBuffer->GetSegment(Segment);
    return QByteArray();
}

QByteArray TorcCameraDevice::GetInitSegment(void)
{
    if (m_ringBuffer)
        return m_ringBuffer->GetInitSegment();
    return QByteArray();
}

TorcCameraParams TorcCameraDevice::GetParams(void)
{
    return m_params;
}

TorcCameraThread::TorcCameraThread(TorcCameraOutput *Parent, const QString &Type, const TorcCameraParams &Params)
  : TorcQThread("Camera"),
    m_parent(Parent),
    m_type(Type),
    m_params(Params),
    m_camera(NULL),
    m_cameraLock(QReadWriteLock::Recursive),
    m_stop(false)
{
    if (m_parent)
    {
        connect(this, SIGNAL(WritingStarted()), m_parent, SLOT(WritingStarted()));
        connect(this, SIGNAL(WritingStopped()), m_parent, SLOT(WritingStopped()));
    }
}

TorcCameraThread::~TorcCameraThread()
{
    QWriteLocker locker(&m_cameraLock);
    if (m_camera)
    {
        delete m_camera;
        m_camera = NULL;
    }
}

void TorcCameraThread::StopWriting(void)
{
    m_stop = true;
}

void TorcCameraThread::Start(void)
{
    if (m_stop)
        return;

    LOG(VB_GENERAL, LOG_INFO, "Camera thread starting");

    m_cameraLock.lockForWrite();
    m_camera = TorcCameraFactory::GetCamera(m_type, m_params);

    if (m_camera)
    {
        if (m_parent)
        {
            connect(m_camera, SIGNAL(InitSegmentReady()),  m_parent, SLOT(InitSegmentReady()));
            connect(m_camera, SIGNAL(SegmentReady(int)),   m_parent, SLOT(SegmentReady(int)));
            connect(m_camera, SIGNAL(SegmentRemoved(int)), m_parent, SLOT(SegmentRemoved(int)));
            connect(m_camera, SIGNAL(SetErrored(bool)),    m_parent, SLOT(CameraErrored(bool)));
        }

        if (m_camera->Setup())
        {
            m_cameraLock.unlock();
            emit WritingStarted();
            m_cameraLock.lockForRead();
            while (!m_stop)
            {
                if (!m_camera->WriteFrame())
                {
                    LOG(VB_GENERAL, LOG_WARNING, "Camera returned error from WriteFrame()");
                    break;
                }
            }
            m_cameraLock.unlock();
            m_cameraLock.lockForWrite();
            m_camera->Stop();
        }

        m_camera->disconnect();
        delete m_camera;
        m_camera = NULL;
    }

    m_cameraLock.unlock();

    LOG(VB_GENERAL, LOG_INFO, "Camera thread stopping");
    emit WritingStopped();
}

void TorcCameraThread::Finish(void)
{
}

QByteArray TorcCameraThread::GetSegment(int Segment)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetSegment(Segment);
    return QByteArray();
}

QByteArray TorcCameraThread::GetInitSegment(void)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetInitSegment();
    return QByteArray();
}

TorcCameraParams TorcCameraThread::GetParams(void)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetParams();
    return TorcCameraParams();
}

TorcCameraOutput::TorcCameraOutput(const QString &ModelId, const QVariantMap &Details)
  : TorcOutput(TorcOutput::Camera, 0.0, ModelId, Details, this, TorcCameraOutput::staticMetaObject,
               "WritingStarted,WritingStopped,CameraErrored,SegmentRemoved,InitSegmentReady,SegmentReady"),
    m_thread(NULL),
    m_threadLock(QReadWriteLock::Recursive),
    m_params(TorcCameraParams(Details)),
    m_segments(),
    m_segmentLock(QReadWriteLock::Recursive),
    m_cameraStartTime()
{
}

TorcCameraOutput::~TorcCameraOutput()
{
    Stop();
}

TorcOutput::Type TorcCameraOutput::GetType(void)
{
    return TorcOutput::Camera;
}

QString TorcCameraOutput::GetPresentationURL(void)
{
    return QString("%1%2").arg(m_signature).arg(VIDEO_PAGE);
}

void TorcCameraOutput::Start(void)
{
    Stop();

    m_threadLock.lockForWrite();
    m_thread = new TorcCameraThread(this, modelId, m_params);
    m_thread->start();
    m_threadLock.unlock();
}

void TorcCameraOutput::InitSegmentReady(void)
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

void TorcCameraOutput::WritingStarted(void)
{
    SetValue(1);
    LOG(VB_GENERAL, LOG_INFO, "Camera started");
}

void TorcCameraOutput::Stop(void)
{
    m_segmentLock.lockForWrite();
    m_segments.clear();
    m_segmentLock.unlock();

    m_threadLock.lockForWrite();
    if (m_thread)
    {
        m_thread->StopWriting();
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = NULL;
    }
    m_threadLock.unlock();
}

void TorcCameraOutput::WritingStopped(void)
{
    SetValue(0);
    LOG(VB_GENERAL, LOG_INFO, "Camera stopped");
    m_cameraStartTime = QDateTime();
}

void TorcCameraOutput::CameraErrored(bool Errored)
{
    SetValid(!Errored);
    if (Errored)
    {
        LOG(VB_GENERAL, LOG_ERR, "Camera reported error");
        Stop();
    }
}

void TorcCameraOutput::SegmentReady(int Segment)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Segment %1 ready").arg(Segment));

    QWriteLocker locker(&m_segmentLock);
    if (!m_segments.contains(Segment))
    {
        if (!m_segments.isEmpty() && m_segments.head() >= Segment)
            LOG(VB_GENERAL, LOG_ERR, QString("Segment %1 is not greater than head (%2)").arg(Segment).arg(m_segments.head()));
        else
            m_segments.enqueue(Segment);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Already have a segment #%1").arg(Segment));
    }
}

void TorcCameraOutput::SegmentRemoved(int Segment)
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

void TorcCameraOutput::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
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
    if (!m_cameraStartTime.isValid())
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
                LOG(VB_GENERAL, LOG_WARNING, "Segment not found");
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

QByteArray TorcCameraOutput::GetPlayerPage(void)
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

QByteArray TorcCameraOutput::GetMasterPlaylist(void)
{
    static const QString playlist("#EXTM3U\r\n"
                                  "#EXT-X-VERSION:4\r\n"
                                  "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%1,RESOLUTION=%2x%3,CODECS=\"%5\"\r\n"
                                  "%4\r\n");

    return QByteArray(playlist.arg(m_params.m_bitrate).arg(m_params.m_width).arg(m_params.m_height)
                                  .arg(HLS_PLAYLIST).arg(m_params.m_videoCodec)/*.arg(AUDIO_CODEC_ISO)*/.toLocal8Bit());
}

QByteArray TorcCameraOutput::GetHLSPlaylist(void)
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

QByteArray TorcCameraOutput::GetDashPlaylist(void)
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
    return QByteArray(dash.arg(m_cameraStartTime.toString(Qt::ISODate))
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
        if (ii.key() == "outputs")
        {
            QVariantMap outputs = ii.value().toMap();
            QVariantMap::const_iterator i = outputs.constBegin();
            for ( ; i != outputs.constEnd(); ++i)
            {
                if (i.key() == "cameras")
                {
                    QVariantMap cameras = i.value().toMap();
                    QVariantMap::iterator it = cameras.begin();
                    for ( ; it != cameras.end(); ++it)
                    {
                        QVariantMap camera = it.value().toMap();
                        if (!camera.contains("name"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' has no name").arg(it.key()));
                            continue;
                        }
                        if (!camera.contains("model"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' does not specify model").arg(camera.value("name").toString()));
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
                        if (!camera.contains("bitrate"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' does not specify bitrate").arg(camera.value("name").toString()));
                            continue;
                        }
                        if (!camera.contains("framerate"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Camera '%1' does not specify framerate").arg(camera.value("name").toString()));
                            continue;
                        }

                        // NB TorcCameraFactory checks that the underlying class can handle the specified camera
                        // which ensures the TorcCameraOutput instance will be able to create the camera.
                        QString model = camera.value("model").toString();
                        TorcCameraOutput *newcamera = NULL;
                        TorcCameraFactory* factory = TorcCameraFactory::GetTorcCameraFactory();
                        TorcCameraParams params(Details);
                        for ( ; factory; factory = factory->NextFactory())
                        {
                            if (factory->CanHandle(it.key(), params))
                            {
                                newcamera = new TorcCameraOutput(model, camera);
                                m_cameras.insertMulti(model, newcamera);
                                LOG(VB_GENERAL, LOG_INFO, QString("New '%1' camera '%2'").arg(model).arg(newcamera->GetUniqueId()));
                                break;
                            }
                        }
                        if (NULL == newcamera)
                            LOG(VB_GENERAL, LOG_WARNING, QString("Failed to find handler for camera '%1'").arg(model));
                    }
                }
            }
        }
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

TorcCameraFactory* TorcCameraFactory::gTorcCameraFactory = NULL;

TorcCameraFactory::TorcCameraFactory()
  : nextTorcCameraFactory(gTorcCameraFactory)
{
    gTorcCameraFactory = this;
}

TorcCameraFactory::~TorcCameraFactory()
{
}

TorcCameraFactory* TorcCameraFactory::GetTorcCameraFactory(void)
{
    return gTorcCameraFactory;
}

TorcCameraFactory* TorcCameraFactory::NextFactory(void) const
{
    return nextTorcCameraFactory;
}

TorcCameraDevice* TorcCameraFactory::GetCamera(const QString &Type, const TorcCameraParams &Params)
{
    TorcCameraDevice* result = NULL;

    TorcCameraFactory* factory = TorcCameraFactory::GetTorcCameraFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        if (factory->CanHandle(Type, Params))
        {
            result = factory->Create(Type, Params);
            break;
        }
    }

    return result;
}
