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
    void             Prepare              (QByteArray &) Q_DECL_OVERRIDE;
    void             Begin                (QByteArray &Dest) Q_DECL_OVERRIDE;
    void             AddProperty          (QByteArray &Dest, const QString &Name, const QVariant &Value) Q_DECL_OVERRIDE;
    void             End                  (QByteArray &Dest) Q_DECL_OVERRIDE;

  private:
    quint64          BinaryFromVariant    (QByteArray &Dest, const QString &Name, const QVariant &Value);
    quint64          BinaryFromStringList (QByteArray &Dest, const QString &Name, const QStringList &Value);
    quint64          BinaryFromArray      (QByteArray &Dest, const QString &Name, const QVariantList &Value);
    quint64          BinaryFromMap        (QByteArray &Dest, const QString &Name, const QVariantMap &Value);
    quint64          BinaryFromQString    (QByteArray &Dest, const QString &Value);
    void             BinaryFromUInt       (QByteArray &Dest, quint64 Value);
    void             BinaryFromUuid       (QByteArray &Dest, const QVariant &Value);
    void             BinaryFromData       (QByteArray &Dest, const QVariant &Value);
    void             CountObjects         (quint64 &Count, const QVariant &Value);

  private:
    quint8                 m_referenceSize;
    QList<quint64>         m_objectOffsets;
    QHash<QString,quint64> m_strings;
};

#endif // TORCBINARYPLISTSERIALISER_H
