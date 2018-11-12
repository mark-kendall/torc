#ifndef TORCPLAINTEXTSERIALISER_H
#define TORCPLAINTEXTSERIALISER_H

// Torc
#include "torcserialiser.h"

class TorcPlainTextSerialiser : public TorcSerialiser
{
  public:
    TorcPlainTextSerialiser();
   ~TorcPlainTextSerialiser();

    HTTPResponseType ResponseType       (void) Q_DECL_OVERRIDE;

  protected:
    void             Prepare            (QByteArray &) Q_DECL_OVERRIDE;
    void             Begin              (QByteArray &) Q_DECL_OVERRIDE;
    void             AddProperty        (QByteArray &Dest, const QString &Name, const QVariant &Value) Q_DECL_OVERRIDE;
    void             End                (QByteArray &) Q_DECL_OVERRIDE;
};

#endif // TORCPLAINTEXTSERIALISER_H
