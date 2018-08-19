#ifndef TORCSEGMENTEDRINGBUFFER_H
#define TORCSEGMENTEDRINGBUFFER_H

// Qt
#include <QPair>
#include <QQueue>
#include <QIODevice>
#include <QReadWriteLock>

class TorcSegmentedRingBuffer : public QObject
{
    Q_OBJECT

  public:
    explicit TorcSegmentedRingBuffer(int Size);
    ~TorcSegmentedRingBuffer();

    int                     GetSize          (void);
    int                     GetSegmentsAvail (int &TailRef);
    int                     Write            (QByteArray    *Data, int Size);
    int                     Write            (const uint8_t *Data, int Size);
    int                     FinishSegment    (bool Init);
    int                     ReadSegment      (uint8_t       *Data, int Max,  int SegmentRef, int Offset = 0);
    int                     ReadSegment      (QIODevice     *Dst,  int SegmentRef);
    QByteArray*             GetSegment       (int SegmentRef);
    QByteArray*             GetInitSegment   (void);
    void                    SaveInitSegment  (void);

  signals:
    void                    SegmentReady     (int Segment);
    void                    SegmentRemoved   (int Segment);

  protected:
    int                     GetBytesFree     (void);

  protected:
    int                     m_size;
    uint8_t*                m_data;
    int                     m_readPosition;
    int                     m_writePosition;
    int                     m_currentSize;
    int                     m_currentStartPosition;
    QReadWriteLock          m_segmentsLock;
    QQueue<QPair<int,int> > m_segments;
    QQueue<int>             m_segmentRefs;
    int                     m_segmentCounter;
    QByteArray*             m_initSegment;

  private:
    Q_DISABLE_COPY(TorcSegmentedRingBuffer)
};

#endif // TORCSEGMENTEDRINGBUFFER_H
