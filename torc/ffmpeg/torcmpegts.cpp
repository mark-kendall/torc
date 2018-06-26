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
#include <QString>

// Torc
#include "torclogging.h"
#include "torcmpegts.h"

// FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
}

#define FFMPEG_BUFFER_SIZE 64*1024

TorcMPEGTS::TorcMPEGTS(TorcSegmentedRingBuffer *Buffer)
  : m_formatCtx(NULL),
    m_created(false),
    m_started(false),
    m_outputFile(),
    m_ringBuffer(Buffer),
    m_ioContext(NULL)
{
    SetupContext();
    SetupIO();
}

TorcMPEGTS::TorcMPEGTS(const QString &File)
  : m_formatCtx(NULL),
    m_created(false),
    m_started(false),
    m_outputFile(File),
    m_ringBuffer(NULL),
    m_ioContext(NULL)
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

bool TorcMPEGTS::AddPacket(AVPacket *Packet, bool CodecConfig)
{
    if (!m_formatCtx || !m_created || !Packet)
        return false;

    if (CodecConfig)
    {
        // copy extradata to video stream
        if ((uint)Packet->stream_index >= m_formatCtx->nb_streams)
        {
            int size = Packet->size;
            AVStream *stream = m_formatCtx->streams[Packet->stream_index];
            stream->codecpar->extradata = (uint8_t*)av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE);
            stream->codecpar->extradata_size = size;
            memcpy(stream->codecpar->extradata, Packet->data, size);
        }

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
        LOG(VB_GENERAL, LOG_ERR, "Failed to write frame");
    return result >= 0;
}

void TorcMPEGTS::Start(void)
{
    if (m_formatCtx)
        if (avformat_write_header(m_formatCtx, NULL))
            LOG(VB_GENERAL, LOG_ERR, "Failed to write stream header");
}

void TorcMPEGTS::Finish(void)
{
    if (m_formatCtx)
    {
        if (av_write_trailer(m_formatCtx))
            LOG(VB_GENERAL, LOG_ERR, "Failed to write stream trailer");
        avio_close(m_formatCtx->pb);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
