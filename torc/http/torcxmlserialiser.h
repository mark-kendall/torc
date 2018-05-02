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

    virtual HTTPResponseType ResponseType    (void) Q_DECL_OVERRIDE;

  protected:
    virtual void             Prepare         (void) Q_DECL_OVERRIDE;
    virtual void             Begin           (void) Q_DECL_OVERRIDE;
    virtual void             AddProperty     (const QString &Name, const QVariant &Value) Q_DECL_OVERRIDE;
    virtual void             End             (void) Q_DECL_OVERRIDE;

    void                     VariantToXML    (const QString &Name, const QVariant &Value);
    void                     ListToXML       (const QString &Name, const QVariantList &Value);
    void                     StringListToXML (const QString &Name, const QStringList &Value);
    void                     MapToXML        (const QString &Name, const QVariantMap &Value);

  protected:
    QXmlStreamWriter m_xmlStream;
    QBuffer          m_buffer;
};

#endif // TORCXMLSERIALISER_H
