#ifndef TORCJSONSERIALISER_H
#define TORCJSONSERIALISER_H

// Qt
#include <QTextStream>

// Torc
#include "torcserialiser.h"

class TorcJSONSerialiser : public TorcSerialiser
{
  public:
    explicit TorcJSONSerialiser(bool Javascript = false);
    virtual ~TorcJSONSerialiser() = default;

    HTTPResponseType ResponseType       (void) override;

  protected:
    void             Prepare            (QByteArray &) override;
    void             Begin              (QByteArray &) override;
    void             AddProperty        (QByteArray &Dest, const QString &Name, const QVariant &Value) override;
    void             End                (QByteArray &) override;

  private:
    Q_DISABLE_COPY(TorcJSONSerialiser)
    bool             m_javaScriptType;
};

#endif // TORCJSONSERIALISER_H
