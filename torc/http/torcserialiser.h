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
    virtual ~TorcSerialiser() {}

    static TorcSerialiser*   GetSerialiser  (const QString &MimeType);
    void                     Serialise      (QByteArray &Dest, const QVariant &Data, const QString &Type = QString());
    virtual HTTPResponseType ResponseType   (void) = 0;

  protected:
    virtual void             Prepare        (QByteArray &Dest) = 0;
    virtual void             Begin          (QByteArray &Dest) = 0;
    virtual void             AddProperty    (QByteArray &Dest, const QString &Name, const QVariant &Value) = 0;
    virtual void             End            (QByteArray &Dest) = 0;
};

class TorcSerialiserFactory
{
  public:
    TorcSerialiserFactory(const QString &Type, const QString &SubType, const QString &Description);
    virtual ~TorcSerialiserFactory();

    static TorcSerialiserFactory* GetTorcSerialiserFactory  (void);
    TorcSerialiserFactory*        NextTorcSerialiserFactory (void) const;
    virtual TorcSerialiser*       Create                    (void) = 0;
    bool                          Accepts                   (const QPair<QString,QString> &MimeType) const;
    const QString&                Description               (void) const;
    QString                       MimeType                  (void) const;

  protected:
    static TorcSerialiserFactory* gTorcSerialiserFactory;
    TorcSerialiserFactory*        m_nextTorcSerialiserFactory;
    QString                       m_type;
    QString                       m_subtype;
    QString                       m_description;

  private:
    Q_DISABLE_COPY(TorcSerialiserFactory)
};

#endif // TORCSERIALISER_H
