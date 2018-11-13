#ifndef TORCPLISTSERIALISER_H
#define TORCPLISTSERIALISER_H

// Torc
#include "torcxmlserialiser.h"

class TorcPListSerialiser : public TorcXMLSerialiser
{
  public:
    TorcPListSerialiser();
    virtual ~TorcPListSerialiser();

    HTTPResponseType ResponseType        (void) override;

  protected:
    void             Begin               (QByteArray &) override;
    void             AddProperty         (QByteArray &, const QString &Name, const QVariant &Value) override;
    void             End                 (QByteArray &) override;

    void             PListFromVariant    (const QString &Name, const QVariant &Value, bool NeedKey = true);
    void             PListFromList       (const QString &Name, const QVariantList &Value);
    void             PListFromStringList (const QString &Name, const QStringList &Value);
    void             PListFromMap        (const QString &Name, const QVariantMap &Value);
};

#endif // TORCPLISTSERIALISER_H
