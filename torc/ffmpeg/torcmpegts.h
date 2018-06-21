#ifndef TORCMPEGTS_H
#define TORCMPEGTS_H

// Qt
#include <QtGlobal>

// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
}

class TorcMPEGTS
{
  public:
    explicit TorcMPEGTS(const QString &File);
    ~TorcMPEGTS();

    bool IsValid        (void);
    int  AddH264Stream  (int Width, int Height, int FrameRate, int Profile, int Bitrate);
    bool AddPacket      (AVPacket *Packet, bool CodecConfig);
    void Finish         (void);

  private:
    void Start          (void);

  private:
    Q_DISABLE_COPY(TorcMPEGTS)
    AVFormatContext     *m_formatCtx;
    bool                 m_created;
    bool                 m_started;
};

#endif // TORCMPEGTS_H
