#ifndef TORCPLISTSERIALISER_H
#define TORCPLISTSERIALISER_H

// Torc
#include "torcxmlserialiser.h"

class TorcPListSerialiser : public TorcXMLSerialiser
{
  public:
    TorcPListSerialiser();
    virtual ~TorcPListSerialiser();

    HTTPResponseType ResponseType        (void) Q_DECL_OVERRIDE;

  protected:
    void             Begin               (QByteArray &) Q_DECL_OVERRIDE;
    void             AddProperty         (QByteArray &, const QString &Name, const QVariant &Value) Q_DECL_OVERRIDE;
    void             End                 (QByteArray &) Q_DECL_OVERRIDE;

    void             PListFromVariant    (const QString &Name, const QVariant &Value, bool NeedKey = true);
    void             PListFromList       (const QString &Name, const QVariantList &Value);
    void             PListFromStringList (const QString &Name, const QStringList &Value);
    void             PListFromMap        (const QString &Name, const QVariantMap &Value);
};

#endif // TORCPLISTSERIALISER_H
