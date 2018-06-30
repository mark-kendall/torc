/* Class TorcMPEGTS
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

// Qt
#include <QMutex>
#include <QString>

// Torc
#include "torclogging.h"
#include "torcmpegts.h"

// FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
}

#define FFMPEG_BUFFER_SIZE 64*1024

static void TorcAVLog(void *Ptr, int Level, const char *Fmt, va_list VAL)
{
    if (VERBOSE_LEVEL_NONE)
        return;

    static QString   line("");
    static const int length = 255;
    static QMutex    lock;
    uint64_t         verboseMask  = VB_GENERAL;
    int              verboseLevel = LOG_DEBUG;

    // determine mythtv debug level from av log level
    switch (Level)
    {
        case AV_LOG_PANIC:
            verboseLevel = LOG_EMERG;
            break;
        case AV_LOG_FATAL:
            verboseLevel = LOG_CRIT;
            break;
        case AV_LOG_ERROR:
            verboseLevel = LOG_ERR;
            break;
        case AV_LOG_DEBUG:
        case AV_LOG_VERBOSE:
            verboseLevel = LOG_DEBUG;
            break;
        case AV_LOG_INFO:
            verboseLevel = LOG_INFO;
            break;
        case AV_LOG_WARNING:
            verboseLevel = LOG_WARNING;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verboseMask, verboseLevel))
        return;

    lock.lock();
    if (line.isEmpty() && Ptr) {
        AVClass* avc = *(AVClass**)Ptr;
        line.sprintf("[%s @ %p] ", avc->item_name(Ptr), avc);
    }

    char str[length+1];
    int bytes = vsnprintf(str, length+1, Fmt, VAL);

    // check for truncated messages and fix them
    if (bytes > length)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Libav log output truncated %1 of %2 bytes written")
                .arg(length).arg(bytes));
        str[length - 1] = '\n';
    }

    line += QString(str);
    if (line.endsWith("\n"))
    {
        LOG(verboseMask, verboseLevel, line.trimmed());
        line.truncate(0);
    }
    lock.unlock();
}

TorcMPEGTS::TorcMPEGTS(TorcSegmentedRingBuffer *Buffer)
  : m_formatCtx(NULL),
    m_created(false),
    m_started(false),
    m_outputFile(),
    m_ringBuffer(Buffer),
    m_ioContext(NULL),
    m_audioContext(NULL),
    m_audioStream(0),
    m_audioFrame(NULL),
    m_audioPacket(NULL),
    m_lastVideoPts(AV_NOPTS_VALUE),
    m_lastAudioPts(AV_NOPTS_VALUE)
{
    av_log_set_callback(TorcAVLog);
    SetupContext();
    SetupIO();
}

TorcMPEGTS::TorcMPEGTS(const QString &File)
  : m_formatCtx(NULL),
    m_created(false),
    m_started(false),
    m_outputFile(File),
    m_ringBuffer(NULL),
    m_ioContext(NULL),
    m_audioContext(NULL),
    m_audioStream(0),
    m_audioFrame(NULL),
    m_audioPacket(NULL),
    m_lastVideoPts(AV_NOPTS_VALUE),
    m_lastAudioPts(AV_NOPTS_VALUE)
{
    SetupContext();
    SetupIO();
}

TorcMPEGTS::~TorcMPEGTS()
{
    // TODO check whether this is safe with file output
    if (m_ioContext)
    {
        if (m_ioContext->buffer)
        {
            av_free(m_ioContext->buffer);
            m_ioContext->buffer = NULL;
        }
        av_free(m_ioContext);
        m_ioContext = NULL;
    }

    if (m_formatCtx)
    {
        avformat_free_context(m_formatCtx);
        m_formatCtx = NULL;
    }

    if (m_audioFrame)
    {
        av_frame_free(&m_audioFrame);
        m_audioFrame = NULL;
    }

    if (m_audioPacket)
    {
        av_packet_free(&m_audioPacket);
        m_audioPacket = NULL;
    }

    if (m_audioContext)
    {
        avcodec_free_context(&m_audioContext);
        m_audioContext = NULL;
    }
}

void TorcMPEGTS::SetupContext(void)
{
    // TODO can this be called once
    av_register_all();

    AVOutputFormat *format = av_guess_format("mpegts", NULL, NULL);
    if (!format)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find MPEGTS muxer");
        return;
    }

    // create context
    m_formatCtx = avformat_alloc_context();
    if (!m_formatCtx)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create AVFormatContext");
        return;
    }
    m_formatCtx->oformat = format;
}

void TorcMPEGTS::SetupIO(void)
{
    if (!m_formatCtx)
        return;

    if (!m_outputFile.isEmpty())
    {
        // create output file
        if (avio_open(&m_formatCtx->pb, m_outputFile.toLocal8Bit().constData(), AVIO_FLAG_WRITE) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' for output").arg(m_outputFile));
            return;
        }
        m_created = true;
        return;
    }

    if (m_ringBuffer)
    {
        uint8_t* iobuffer  = (uint8_t *)av_malloc(FFMPEG_BUFFER_SIZE);
        m_ioContext        = avio_alloc_context(iobuffer, FFMPEG_BUFFER_SIZE, 1, this, NULL, &AVWritePacket, NULL);
        m_formatCtx->pb    = m_ioContext;
        m_formatCtx->flags = AVFMT_FLAG_CUSTOM_IO;
        m_created = true;
        return;
    }
}

int TorcMPEGTS::AVWritePacket(void *Opaque, uint8_t *Buffer, int Size)
{
    TorcMPEGTS* muxer = static_cast<TorcMPEGTS*>(Opaque);
    if (!muxer)
        return -1;

    return muxer->WriteAVPacket(Buffer, Size);
}

int TorcMPEGTS::WriteAVPacket(uint8_t *Buffer, int Size)
{
    if (!Buffer || Size < 1)
        return -1;

    if (m_ringBuffer)
        return m_ringBuffer->Write(Buffer, Size);

    return -1;
}

bool TorcMPEGTS::IsValid(void)
{
    return m_formatCtx && m_formatCtx->nb_streams > 0 && m_created;
}

int TorcMPEGTS::AddDummyAudioStream(void)
{
    if (!m_formatCtx)
        return -1;

    AVStream *audiostream = avformat_new_stream(m_formatCtx, 0);
    if (!audiostream)
        return -1;

    m_audioStream                         = m_formatCtx->nb_streams - 1;
    audiostream->id                       = m_formatCtx->nb_streams - 1;
    audiostream->codecpar->codec_id       = AV_CODEC_ID_AAC;
    audiostream->codecpar->codec_type     = AVMEDIA_TYPE_AUDIO;
    audiostream->codecpar->codec_tag      = 0;
    audiostream->codecpar->bit_rate       = 64000;
    audiostream->codecpar->sample_rate    = 44100;
    audiostream->codecpar->channels       = 2;
    audiostream->codecpar->channel_layout = AV_CH_LAYOUT_STEREO;

    LOG(VB_GENERAL, LOG_INFO, QString("Audio stream id %1").arg(audiostream->id));
    AVCodec* codec = avcodec_find_encoder(audiostream->codecpar->codec_id);
    if (!codec)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find dummy audio codec");
        return -1;
    }
    LOG(VB_GENERAL, LOG_INFO, QString("Found audio codec '%1'").arg(codec->name));

    m_audioContext = avcodec_alloc_context3(codec);
    if (!m_audioContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create audio context");
        return -1;
    }
    LOG(VB_GENERAL, LOG_INFO, "Created audio context");

    avcodec_parameters_to_context(m_audioContext, audiostream->codecpar);
    m_audioContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_audioContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(m_audioContext, codec, NULL) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open codec '%1'").arg(codec->name));
        return -1;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Dummy audio: frame size %1 sample_fmt %2 sample_rate %3 bitrate %4 channels %5")
        .arg(m_audioContext->frame_size).arg(m_audioContext->sample_fmt).arg(m_audioContext->sample_rate)
        .arg(m_audioContext->bit_rate).arg(m_audioContext->channels));

    m_audioPacket                = av_packet_alloc();
    m_audioFrame                 = av_frame_alloc();
    m_audioFrame->nb_samples     = m_audioContext->frame_size;
    m_audioFrame->format         = m_audioContext->sample_fmt;
    m_audioFrame->channel_layout = m_audioContext->channel_layout;
    CopyExtraData(m_audioContext->extradata_size, m_audioContext->extradata, m_audioStream);
    if (av_frame_get_buffer(m_audioFrame, 0) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create audio frame buffers");
        return -1;
    }
    return audiostream->id;
}

void TorcMPEGTS::WriteDummyAudio(void)
{
    if (!m_audioContext || !m_audioFrame || !m_audioPacket || m_lastVideoPts == AV_NOPTS_VALUE || !m_formatCtx)
        return;

    while (1)
    {
        if (avcodec_send_frame(m_audioContext, m_audioFrame) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Error sending frame to audio encoder");
            break;
        }
        else
        {
            int rec = avcodec_receive_packet(m_audioContext, m_audioPacket);
            if (rec == AVERROR(EAGAIN))
                continue;
            if (rec < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, "Error receiving packet from audio encoder");
                break;
            }
            while ((m_lastVideoPts > m_lastAudioPts) || m_lastAudioPts == AV_NOPTS_VALUE)
            {
                int64_t duration = ((float)m_audioContext->frame_size / 44100.0) * 90000;
                m_lastAudioPts = m_lastAudioPts == AV_NOPTS_VALUE ? m_lastVideoPts : m_lastAudioPts + duration;
                m_audioPacket->pts = m_lastAudioPts;
                m_audioPacket->dts = m_lastAudioPts;
                m_audioPacket->stream_index = m_audioStream;
                if (av_write_frame(m_formatCtx, m_audioPacket) < 0)
                    LOG(VB_GENERAL, LOG_ERR, "Failed to write audio frame to muxer");
            }
            av_packet_unref(m_audioPacket);
            break;
        }
    }
}

int TorcMPEGTS::AddH264Stream(int Width, int Height, int Profile, int Bitrate)
{
    if (!m_formatCtx)
        return -1;

    AVStream *h264video = avformat_new_stream(m_formatCtx, 0);
    if (!h264video)
        return -1;

    h264video->id = m_formatCtx->nb_streams - 1;
    h264video->codecpar->codec_id            = AV_CODEC_ID_H264;
    h264video->codecpar->codec_type          = AVMEDIA_TYPE_VIDEO;
    h264video->codecpar->codec_tag           = 0;
    h264video->codecpar->bit_rate            = Bitrate;
    h264video->codecpar->profile             = Profile;
    h264video->codecpar->level               = 30; // does this need to be set properly?
    h264video->codecpar->format              = AV_PIX_FMT_YUV420P;
    h264video->codecpar->width               = Width;
    h264video->codecpar->height              = Height;
    return h264video->id;
}

void TorcMPEGTS::CopyExtraData(int Size, void* Source, int Stream)
{
    if (!m_formatCtx || !Source || Size < 1)
        return;

    if (m_formatCtx->nb_streams <= 0 || (uint)Stream >= m_formatCtx->nb_streams || Stream < 0)
    {
        LOG(VB_GENERAL, LOG_INFO, "Cannot copy extradata - invalid stream");
        return;
    }

    AVStream *stream = m_formatCtx->streams[Stream];
    stream->codecpar->extradata = (uint8_t*)av_mallocz(Size + AV_INPUT_BUFFER_PADDING_SIZE);
    stream->codecpar->extradata_size = Size;
    memcpy(stream->codecpar->extradata, Source, Size);
}

bool TorcMPEGTS::AddPacket(AVPacket *Packet, bool CodecConfig)
{
    if (!m_formatCtx || !m_created || !Packet)
        return false;

    if (CodecConfig)
    {
        CopyExtraData(Packet->size, Packet->data, Packet->stream_index);

        // and start muxer if needed
        if (!m_started)
        {
            Start();
            m_started = true;
        }
    }

    if (!m_started)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Ignoring packet - stream not started (waiting for config?)");
        return true;
    }

    int result = av_write_frame(m_formatCtx, Packet);
    if (result < 0)
        LOG(VB_GENERAL, LOG_ERR, "Failed to write video frame");
    else
        m_lastVideoPts = Packet->pts;

    WriteDummyAudio();

    return result >= 0;
}

void TorcMPEGTS::Start(void)
{
    if (m_formatCtx)
    {
        av_dump_format(m_formatCtx, 0, "stdout", 1);
        if (avformat_write_header(m_formatCtx, NULL))
            LOG(VB_GENERAL, LOG_ERR, "Failed to write stream header");
    }
}

void TorcMPEGTS::Finish(void)
{
    if (m_audioContext && m_audioPacket)
    {
        // flush
        avcodec_send_frame(m_audioContext, NULL);
        while (avcodec_receive_packet(m_audioContext, m_audioPacket) >= 0)
            av_packet_unref(m_audioPacket);
    }

    if (m_formatCtx)
    {
        if (av_write_trailer(m_formatCtx))
            LOG(VB_GENERAL, LOG_ERR, "Failed to write stream trailer");
        //avio_close(m_formatCtx->pb);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
