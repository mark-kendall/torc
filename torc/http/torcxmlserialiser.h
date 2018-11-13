#ifndef TORCXMLSERIALISER_H
#define TORCXMLSERIALISER_H

// Qt
#include <QBuffer>
#include <QXmlStreamWriter>

// Torc
#include "torcserialiser.h"

class TorcXMLSerialiser : public TorcSerialiser
{
  public:
    TorcXMLSerialiser();
    virtual ~TorcXMLSerialiser();

    virtual HTTPResponseType ResponseType    (void) override;

  protected:
    virtual void             Prepare         (QByteArray &Dest) override;
    virtual void             Begin           (QByteArray &Dest) override;
    virtual void             AddProperty     (QByteArray &Dest, const QString &Name, const QVariant &Value) override;
    virtual void             End             (QByteArray &Dest) override;

    void                     VariantToXML    (const QString &Name, const QVariant &Value);
    void                     ListToXML       (const QString &Name, const QVariantList &Value);
    void                     StringListToXML (const QString &Name, const QStringList &Value);
    void                     MapToXML        (const QString &Name, const QVariantMap &Value);

  protected:
    QXmlStreamWriter m_xmlStream;
    QBuffer          m_buffer;
};

#endif // TORCXMLSERIALISER_H
