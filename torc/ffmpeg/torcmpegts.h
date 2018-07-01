#ifndef TORCMPEGTS_H
#define TORCMPEGTS_H

// Qt
#include <QtGlobal>

// Torc
#include "torcsegmentedringbuffer.h"

// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
}

class TorcMPEGTS
{
  public:
    static int AVWritePacket(void *Opaque, uint8_t *Buffer, int Size);

    explicit TorcMPEGTS     (const QString &File);
    explicit TorcMPEGTS     (TorcSegmentedRingBuffer *Buffer);
    ~TorcMPEGTS();

    bool IsValid            (void);
    int  AddH264Stream      (int Width, int Height, int Profile, int Bitrate);
    int  AddDummyAudioStream(void);
    bool AddPacket          (AVPacket *Packet, bool CodecConfig);
    void FinishSegment      (void);
    void Finish             (void);
    int  WriteAVPacket      (uint8_t* Buffer, int Size);

  private:
    void SetupContext       (void);
    void SetupIO            (void);
    void Start              (void);
    void WriteDummyAudio    (void);
    void CopyExtraData      (int Size, void* Source, int Stream);

  private:
    Q_DISABLE_COPY(TorcMPEGTS)
    // Output muxer
    AVFormatContext         *m_formatCtx;
    bool                     m_created;
    bool                     m_started;
    // Output buffers/file
    QString                  m_outputFile;
    TorcSegmentedRingBuffer *m_ringBuffer;
    AVIOContext             *m_ioContext;
    // Dummy audio generation
    AVCodecContext          *m_audioContext;
    int                      m_audioStream;
    AVFrame                 *m_audioFrame;
    AVPacket                *m_audioPacket;
    // AV Sync
    int64_t                  m_lastVideoPts;
    int64_t                  m_lastAudioPts;
};

#endif // TORCMPEGTS_H
