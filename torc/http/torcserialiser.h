#ifndef TORCSERIALISER_H
#define TORCSERIALISER_H

// Qt
#include <QVariant>
#include <QStringList>
#include <QMap>
#include <QDateTime>

// Torc
#include "torchttprequest.h"

#define QVARIANT_ERROR QString("Error: QVariantList must contain only one variant type")

class TorcSerialiser
{
  public:
    TorcSerialiser();
    virtual ~TorcSerialiser();

    static TorcSerialiser*   GetSerialiser  (const QString &MimeType);
    QByteArray*              Serialise      (const QVariant &Data, const QString &Type);
    virtual HTTPResponseType ResponseType   (void) = 0;

  protected:
    virtual void             Prepare        (void) = 0;
    virtual void             Begin          (void) = 0;
    virtual void             AddProperty    (const QString &Name, const QVariant &Value) = 0;
    virtual void             End            (void) = 0;

  protected:
    QByteArray *m_content;

  private:
    // disable copy and assignment constructors
    TorcSerialiser(const TorcSerialiser &) Q_DECL_EQ_DELETE;
    TorcSerialiser &operator=(const TorcSerialiser &) Q_DECL_EQ_DELETE;
};

class TorcSerialiserFactory
{
  public:
    TorcSerialiserFactory(const QString &Accepts, const QString &Description);
    virtual ~TorcSerialiserFactory();

    static TorcSerialiserFactory* GetTorcSerialiserFactory  (void);
    TorcSerialiserFactory*        NextTorcSerialiserFactory (void) const;
    virtual TorcSerialiser*       Create                    (void) = 0;
    const QString&                Accepts                   (void) const;
    const QString&                Description               (void) const;

  protected:
    static TorcSerialiserFactory* gTorcSerialiserFactory;
    TorcSerialiserFactory*        m_nextTorcSerialiserFactory;
    QString                       m_accepts;
    QString                       m_description;
};

#endif // TORCSERIALISER_H
