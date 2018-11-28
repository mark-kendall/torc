#ifndef TORCMATHS_H
#define TORCMATHS_H

// Qt
#include <QQueue>

/*! \brief Compute a running average.
 *
 * If MaxCount is set, the average will be limited to MaxCount number of most recent samples.
*/
template <typename T> class TorcAverage
{
  public:
    explicit TorcAverage(int MaxCount = 0)
      : m_average(0.0),
        m_count(0),
        m_maxCount(MaxCount),
        m_values()
    {
    }

    double AddValue(T Value)
    {
        if (m_maxCount)
        {
            while (m_values.size() >= m_maxCount)
            {
                m_average = ((m_average * m_count) - m_values.dequeue()) / (m_count - 1);
                m_count--;
            }
            m_values.enqueue(Value);
        }
        m_average = ((m_average * m_count) + Value) / (m_count + 1);
        m_count++;
        return m_average;
    }

    double GetAverage(void)
    {
        return m_average;
    }

    void Reset(void)
    {
        m_average = 0.0;
        m_count = 0;
        m_values.clear();
    }

  private:
    double    m_average;
    quint64   m_count;
    int       m_maxCount;
    QQueue<T> m_values;
};

#endif // TORCMATHS_H
