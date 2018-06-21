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

TorcMPEGTS::TorcMPEGTS(const QString &File)
  : m_formatCtx(NULL),
    m_created(false),
    m_started(false)
{
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

    // create output file
    if (avio_open(&m_formatCtx->pb, File.toLocal8Bit().constData(), AVIO_FLAG_WRITE) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' for output").arg(File));
        return;
    }

    m_created = true;
}

TorcMPEGTS::~TorcMPEGTS()
{
    avformat_free_context(m_formatCtx);
}

bool TorcMPEGTS::IsValid(void)
{
    return m_formatCtx && m_formatCtx->nb_streams > 0 && m_created;
}

int TorcMPEGTS::AddH264Stream(int Width, int Height, int FrameRate, int Profile, int Bitrate)
{
    if (!m_formatCtx)
        return -1;

    AVStream *h264video = avformat_new_stream(m_formatCtx, 0);
    if (!h264video)
        return -1;

    // NB much of the frame rate/aspect ratio setting is redundant. The muxer
    // does not appear to use it but the output stream must have valid SPS/PPS -including SPS timing data.
    av_stream_set_r_frame_rate(h264video, (AVRational){FrameRate, 1});
    h264video->id = m_formatCtx->nb_streams - 1;
    h264video->avg_frame_rate                = (AVRational){1, FrameRate};
    h264video->time_base                     = (AVRational){1, FrameRate};
    h264video->sample_aspect_ratio           = (AVRational){1, 1};  // NB Hardcoded atm
    h264video->display_aspect_ratio          = (AVRational){16, 9}; // NB Hardcoded atm
    h264video->codecpar->sample_aspect_ratio = (AVRational){1, 1};  // NB Hardcoded atm
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

    return av_write_frame(m_formatCtx, Packet) > 0;
}

void TorcMPEGTS::Start(void)
{
    if (m_formatCtx)
        if (avformat_write_header(m_formatCtx, NULL))
            LOG(VB_GENERAL, LOG_ERR, "Failed to write stream header");
}

void TorcMPEGTS::Finish(void)
{
    av_write_trailer(m_formatCtx);
    avio_close(m_formatCtx->pb);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
