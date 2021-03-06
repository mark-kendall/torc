#ifndef TORCCOREUTILS_H
#define TORCCOREUTILS_H

// Qt
#include <QFile>
#include <QMetaEnum>
#include <QDateTime>

namespace TorcCoreUtils
{
    QDateTime   DateTimeFromString    (const QString &String);
    quint64     GetMicrosecondCount   (void);
    void        QtMessage             (QtMsgType Type, const QMessageLogContext &Context, const QString &Message);
    bool        HasZlib               (void);
    QByteArray  GZipCompress          (QByteArray &Source);
    QByteArray  GZipCompressFile      (QFile &Source);

    template <typename T> QString EnumToLowerString(T Value)
    {
        return QString(QMetaEnum::fromType<T>().valueToKey(Value)).toLower();
    }

    template <typename T> QString EnumToString(T Value)
    {
        return QMetaEnum::fromType<T>().valueToKey(Value);
    }

    template <typename T> int StringToEnum(const QString &Value, bool CaseSensitive = false)
    {
        const QMetaEnum metaEnum = QMetaEnum::fromType<T>();
        if (CaseSensitive)
            return metaEnum.keyToValue(Value.toLatin1());
        QByteArray value = Value.toLower().toLatin1();
        for (int count = metaEnum.keyCount() - 1 ; count >= 0; --count)
            if (qstrcmp(value, QByteArray(metaEnum.key(count)).toLower()) == 0)
                return metaEnum.value(count);
        return -1; // for consistency with QMetaEnum::keyToValue
    }

    template <typename T> QStringList EnumList()
    {
        const QMetaEnum metaEnum = QMetaEnum::fromType<T>();
        QStringList result;
        result.reserve(metaEnum.keyCount());
        for (int count = metaEnum.keyCount() - 1 ; count >= 0; --count)
            result << QString(metaEnum.key(count)).toLower();
        return result;
    }
}

#endif // TORCCOREUTILS_H
