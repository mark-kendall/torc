#ifndef TORCBINARYPLISTSERIALISER_H
#define TORCBINARYPLISTSERIALISER_H

// Qt
#include <QDataStream>

// Torc
#include "torcserialiser.h"

class TorcBinaryPListSerialiser : public TorcSerialiser
{
  public:
    TorcBinaryPListSerialiser();
    virtual ~TorcBinaryPListSerialiser();

    HTTPResponseType ResponseType         (void) Q_DECL_OVERRIDE;

  protected:
    void             Prepare              (void) Q_DECL_OVERRIDE;
    void             Begin                (void) Q_DECL_OVERRIDE;
    void             AddProperty          (const QString &Name, const QVariant &Value) Q_DECL_OVERRIDE;
    void             End                  (void) Q_DECL_OVERRIDE;

  private:
    quint64          BinaryFromVariant    (const QString &Name, const QVariant &Value);
    quint64          BinaryFromStringList (const QString &Name, const QStringList &Value);
    quint64          BinaryFromArray      (const QString &Name, const QVariantList &Value);
    quint64          BinaryFromMap        (const QString &Name, const QVariantMap &Value);
    quint64          BinaryFromQString    (const QString &Value);
    void             BinaryFromUInt       (quint64 Value);
    void             CountObjects         (quint64 &Count, const QVariant &Value);

  private:
    quint8                 m_referenceSize;
    QTextCodec            *m_codec;
    QList<quint64>         m_objectOffsets;
    QHash<QString,quint64> m_strings;

  private:
    Q_DISABLE_COPY(TorcBinaryPListSerialiser)
};

#endif // TORCBINARYPLISTSERIALISER_H
