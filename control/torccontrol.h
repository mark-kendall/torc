#ifndef TORCCONTROL_H
#define TORCCONTROL_H

// Qt
#include <QObject>

class TorcControl : public QObject
{
    Q_OBJECT

  public:
    TorcControl();
    virtual ~TorcControl();
};

#endif // TORCCONTROL_H
