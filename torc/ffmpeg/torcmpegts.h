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
    bool AddPacket          (AVPacket *Packet, bool CodecConfig);
    void Finish             (void);
    int  WriteAVPacket      (uint8_t* Buffer, int Size);

  private:
    void SetupContext       (void);
    void SetupIO            (void);
    void Start              (void);

  private:
    Q_DISABLE_COPY(TorcMPEGTS)
    AVFormatContext         *m_formatCtx;
    bool                     m_created;
    bool                     m_started;
    QString                  m_outputFile;
    TorcSegmentedRingBuffer *m_ringBuffer;
    AVIOContext             *m_ioContext;
};

#endif // TORCMPEGTS_H
