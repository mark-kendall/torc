#ifndef TORCPLAINTEXTSERIALISER_H
#define TORCPLAINTEXTSERIALISER_H

// Torc
#include "torcserialiser.h"

class TorcPlainTextSerialiser : public TorcSerialiser
{
  public:
    TorcPlainTextSerialiser();
   ~TorcPlainTextSerialiser();

    HTTPResponseType ResponseType       (void) override;

  protected:
    void             Prepare            (QByteArray &) override;
    void             Begin              (QByteArray &) override;
    void             AddProperty        (QByteArray &Dest, const QString &Name, const QVariant &Value) override;
    void             End                (QByteArray &) override;
};

#endif // TORCPLAINTEXTSERIALISER_H
