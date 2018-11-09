/* Class TorcBinaryPListSerialiser
*
* Copyright (C) Mark Kendall 2012-18
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QTextCodec>
#include <QtEndian>

// Torc
#include "torclogging.h"
#include "torcplist.h"
#include "torcbinaryplistserialiser.h"

#include <stdint.h>

#define START_OBJECT { m_objectOffsets.append(Dest.size()); }

/*! \class TorcBinaryPListSerialiser
 *  \brief Data serialiser for the Apple binary property list format
 *
 * \todo Top level dictionary
*/

TorcBinaryPListSerialiser::TorcBinaryPListSerialiser()
  : TorcSerialiser(),
    m_referenceSize(8),
    m_objectOffsets(),
    m_strings()
{
}

TorcBinaryPListSerialiser::~TorcBinaryPListSerialiser()
{
}

static inline void WriteReference(quint64 Reference, quint8 Size, quint8* Pointer, quint64 &Counter)
{
    switch (Size)
    {
        case 1:
        {
            Pointer[Counter++] = (quint8)(Reference & 0xff);
            return;
        }
        case 2:
        {
            quint16 buffer = qToBigEndian((quint16)(Reference & 0xffff));
            for (int i = 0; i < 2; ++i)
                Pointer[Counter++] = *((quint8*)&buffer + i);
            return;
        }
        case 4:
        {
            quint32 buffer = qToBigEndian((quint32)(Reference & 0xffffffff));
            for (int i = 0; i < 4; ++i)
                Pointer[Counter++] = *((quint8*)&buffer + i);
            return;
        }
        case 8:
        {
            quint64 buffer = qToBigEndian(Reference);
            for (int i = 0; i < 8; ++i)
                Pointer[Counter++] = *((quint8*)&buffer + i);
            return;
        }
    }
}

HTTPResponseType TorcBinaryPListSerialiser::ResponseType(void)
{
    return HTTPResponseBinaryPList;
}

void TorcBinaryPListSerialiser::Prepare(QByteArray &)
{
}

void TorcBinaryPListSerialiser::Begin(QByteArray &Dest)
{
    Dest.reserve(1024);
    m_objectOffsets.clear();
    Dest.append("bplist00");
}

void TorcBinaryPListSerialiser::AddProperty(QByteArray &Dest, const QString &Name, const QVariant &Value)
{
    // this is the maximum count before string optimisation
    quint64 count = 2;
    CountObjects(count, Value);

    LOG(VB_GENERAL, LOG_INFO, QString("Max object count %1").arg(count));

    m_referenceSize = count < 0x000000ff ? 1 :
                      count < 0x0000ffff ? 2 :
                      count < 0xffffffff ? 4 : 8;

    START_OBJECT
    Dest.append(0xd1);
    quint64 offset = Dest.size();
    QByteArray references(2 * m_referenceSize, 0);
    Dest.append(references);

    WriteReference(BinaryFromQString(Dest, Name), m_referenceSize, (quint8*)Dest.data(), offset);
    WriteReference(BinaryFromVariant(Dest, Name, Value), m_referenceSize, (quint8*)Dest.data(), offset);
}

void TorcBinaryPListSerialiser::End(QByteArray &Dest)
{
    quint64 table = Dest.size();

    quint8 offsetsize = table < 0x000000ff ? 1 :
                        table < 0x0000ffff ? 2 :
                        table < 0xffffffff ? 4 : 8;

    QList<quint64>::iterator it = m_objectOffsets.begin();
    switch (offsetsize)
    {
        case 1:
        {
            for ( ; it != m_objectOffsets.end(); ++it)
                Dest.append((quint8)((*it) & 0xff));
            break;
        }
        case 2:
        {
            for ( ; it != m_objectOffsets.end(); ++it)
            {
                quint16 buffer = qToBigEndian((quint16)((*it) & 0xffff));
                Dest.append(*((quint8*)&buffer));
                Dest.append(*((quint8*)&buffer + 1));
            }
            break;
        }
        case 4:
        {
            for ( ; it != m_objectOffsets.end(); ++it)
            {
                quint32 buffer= qToBigEndian((quint32)((*it) & 0xffffffff));
                Dest.append(*((quint8*)&buffer));
                Dest.append(*((quint8*)&buffer + 1));
                Dest.append(*((quint8*)&buffer + 2));
                Dest.append(*((quint8*)&buffer + 3));
            }
            break;
        }
        case 8:
        {
            for ( ; it != m_objectOffsets.end(); ++it)
            {
                quint64 buffer = qToBigEndian((*it));
                Dest.append(*((quint8*)&buffer));
                Dest.append(*((quint8*)&buffer + 1));
                Dest.append(*((quint8*)&buffer + 2));
                Dest.append(*((quint8*)&buffer + 3));
                Dest.append(*((quint8*)&buffer + 4));
                Dest.append(*((quint8*)&buffer + 5));
                Dest.append(*((quint8*)&buffer + 6));
                Dest.append(*((quint8*)&buffer + 7));
            }
            break;
        }
        default:
            break;
    }

    table = qToBigEndian(table);
    quint64 count = qToBigEndian((quint64)m_objectOffsets.count());

    QByteArray trailer(32, 0);
    trailer[6]  = offsetsize;
    trailer[7]  = m_referenceSize;
    trailer[8]  = *((quint8*)&count);
    trailer[9]  = *((quint8*)&count + 1);
    trailer[10] = *((quint8*)&count + 2);
    trailer[11] = *((quint8*)&count + 3);
    trailer[12] = *((quint8*)&count + 4);
    trailer[13] = *((quint8*)&count + 5);
    trailer[14] = *((quint8*)&count + 6);
    trailer[15] = *((quint8*)&count + 7);
    trailer[24] = *((quint8*)&table);
    trailer[25] = *((quint8*)&table + 1);
    trailer[26] = *((quint8*)&table + 2);
    trailer[27] = *((quint8*)&table + 3);
    trailer[28] = *((quint8*)&table + 4);
    trailer[29] = *((quint8*)&table + 5);
    trailer[30] = *((quint8*)&table + 6);
    trailer[31] = *((quint8*)&table + 7);

    Dest.append(trailer);

    LOG(VB_GENERAL, LOG_INFO, QString("Actual object count %1").arg(m_objectOffsets.count()));

#if 1
    QByteArray testdata(Dest.data(), Dest.size());
    TorcPList test(testdata);
    LOG(VB_GENERAL, LOG_INFO, "\n" + test.ToString());
#endif
}

quint64 TorcBinaryPListSerialiser::BinaryFromVariant(QByteArray &Dest, const QString &Name, const QVariant &Value)
{
    // object formats not used; null, fill, data, ascii string, uid, set

    quint64 result = m_objectOffsets.size();

    switch ((int)Value.type())
    {
        case QMetaType::QVariantList: return BinaryFromArray(Dest, Name, Value.toList());
        case QMetaType::QStringList:  return BinaryFromStringList(Dest, Name, Value.toStringList());
        case QMetaType::QVariantMap:  return BinaryFromMap(Dest, Name, Value.toMap());
        case QMetaType::Bool:
        {
            START_OBJECT
            bool value = Value.toBool();
            Dest.append((quint8)(value ? 0x09 : 0x08));
            return result;
        }
        case QMetaType::Int:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::Float:
        case QMetaType::Double:
        {
            START_OBJECT
            double value = Value.toDouble();
            Dest.append((quint8)0x23);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN && !defined (__VFP_FP__)
            Dest.append(*((char*)&value));
            Dest.append(*((char*)&value + 1));
            Dest.append(*((char*)&value + 2));
            Dest.append(*((char*)&value + 3));
            Dest.append(*((char*)&value + 4));
            Dest.append(*((char*)&value + 5));
            Dest.append(*((char*)&value + 6));
            Dest.append(*((char*)&value + 7));
#else
            Dest.append(*((char*)&value + 7));
            Dest.append(*((char*)&value + 6));
            Dest.append(*((char*)&value + 5));
            Dest.append(*((char*)&value + 4));
            Dest.append(*((char*)&value + 3));
            Dest.append(*((char*)&value + 2));
            Dest.append(*((char*)&value + 1));
            Dest.append(*((char*)&value));
#endif
            return result;
        }
        case QMetaType::UInt:
        case QMetaType::UShort:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
        {
            START_OBJECT
            BinaryFromUInt(Dest, Value.toULongLong());
            return result;
        }
        case QMetaType::QDateTime:
        {
            START_OBJECT
            Dest.append(0x33);
            double value = (double)Value.toDateTime().toTime_t();
#if Q_BYTE_ORDER == Q_BIG_ENDIAN && !defined (__VFP_FP__)
            Dest.append(*((char*)&value));
            Dest.append(*((char*)&value + 1));
            Dest.append(*((char*)&value + 2));
            Dest.append(*((char*)&value + 3));
            Dest.append(*((char*)&value + 4));
            Dest.append(*((char*)&value + 5));
            Dest.append(*((char*)&value + 6));
            Dest.append(*((char*)&value + 7));
#else
            Dest.append(*((char*)&value + 7));
            Dest.append(*((char*)&value + 6));
            Dest.append(*((char*)&value + 5));
            Dest.append(*((char*)&value + 4));
            Dest.append(*((char*)&value + 3));
            Dest.append(*((char*)&value + 2));
            Dest.append(*((char*)&value + 1));
            Dest.append(*((char*)&value));
#endif
            return result;
        }
        case QMetaType::QString:
        default:
            return BinaryFromQString(Dest, Value.toString());
    }
}

quint64 TorcBinaryPListSerialiser::BinaryFromStringList(QByteArray &Dest, const QString &Name, const QStringList &Value)
{
    (void)Name;

    quint64 result = m_objectOffsets.size();
    START_OBJECT

    int size = Value.size();
    Dest.append((quint8)(0xa0 | (size < 0xf ? size : 0xf)));
    if (size > 0xe)
        BinaryFromUInt(Dest, size);

    quint64 offset = Dest.size();
    QByteArray references(size * m_referenceSize, 0);
    Dest.append(references);

    QStringList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        WriteReference(BinaryFromQString(Dest, (*it)), m_referenceSize, (quint8*)Dest.data(), offset);

    return result;
}

quint64 TorcBinaryPListSerialiser::BinaryFromArray(QByteArray &Dest, const QString &Name, const QVariantList &Value)
{
    if (!Value.isEmpty())
    {
        int type = Value[0].type();

        QVariantList::const_iterator it = Value.begin();
        for ( ; it != Value.end(); ++it)
            if ((int)(*it).type() != type)
                return BinaryFromQString(Dest, "Error: QVariantList is not valid service return type");
    }

    quint64 result = m_objectOffsets.size();
    START_OBJECT

    int size = Value.size();
    Dest.append((quint8)(0xa0 | (size < 0xf ? size : 0xf)));
    if (size > 0xe)
        BinaryFromUInt(Dest, size);

    quint64 offset = Dest.size();
    QByteArray references(size * m_referenceSize, 0);
    Dest.append(references);

    QVariantList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        WriteReference(BinaryFromVariant(Dest, Name, (*it)), m_referenceSize, (quint8*)Dest.data(), offset);

    return result;
}

quint64 TorcBinaryPListSerialiser::BinaryFromMap(QByteArray &Dest, const QString &Name, const QVariantMap &Value)
{
    (void)Name;

    quint64 result = m_objectOffsets.size();
    START_OBJECT

    int size = Value.size();
    Dest.append((quint8)(0xd0 | (size < 0xf ? size : 0xf)));
    if (size > 0xe)
        BinaryFromUInt(Dest, size);

    quint64 offset = Dest.size();
    QByteArray references(size * 2 * m_referenceSize, 0);
    Dest.append(references);

    QVariantMap::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        WriteReference(BinaryFromQString(Dest, it.key()), m_referenceSize, (quint8*)Dest.data(), offset);

    it = Value.begin();
    for ( ; it != Value.end(); ++it)
        WriteReference(BinaryFromVariant(Dest, it.key(), it.value()), m_referenceSize, (quint8*)Dest.data(), offset);

    return result;
}

quint64 TorcBinaryPListSerialiser::BinaryFromQString(QByteArray &Dest, const QString &Value)
{
    static QTextCodec* textCodec = QTextCodec::codecForName("UTF-16BE");

    QHash<QString,quint64>::const_iterator it = m_strings.find(Value);
    if (it != m_strings.end())
        return it.value();

    quint64 result = (quint64)m_objectOffsets.size();
    m_strings.insert(Value, result);

    START_OBJECT

    QByteArray output = textCodec->fromUnicode(Value);

    quint64 size = (output.size() >> 1) - 1;

    Dest.append((quint8)(0x60 | (size < 0xf ? size : 0xf)));
    if (size > 0xe)
        BinaryFromUInt(Dest, size);

    Dest.append(output.data() + 2, output.size() - 2);
    return result;
}

void TorcBinaryPListSerialiser::BinaryFromUInt(QByteArray &Dest, quint64 Value)
{
    uint8_t size = Value <= UINT8_MAX ? 0 : Value <= UINT16_MAX ? 1 : Value <= UINT32_MAX ? 2 : 3;
    Dest.append((quint8)(0x10 | size));

    switch (size)
    {
        case 3:
        {
            quint64 buffer = qToBigEndian(Value);
            Dest.append(*((quint8*)&buffer));
            Dest.append(*((quint8*)&buffer + 1));
            Dest.append(*((quint8*)&buffer + 2));
            Dest.append(*((quint8*)&buffer + 3));
            Dest.append(*((quint8*)&buffer + 4));
            Dest.append(*((quint8*)&buffer + 5));
            Dest.append(*((quint8*)&buffer + 6));
            Dest.append(*((quint8*)&buffer + 7));
            break;
        }
        case 2:
        {
            quint32 buffer= qToBigEndian((quint32)(Value & 0xffffffff));
            Dest.append(*((quint8*)&buffer));
            Dest.append(*((quint8*)&buffer + 1));
            Dest.append(*((quint8*)&buffer + 2));
            Dest.append(*((quint8*)&buffer + 3));
            break;
        }
        case 1:
        {
            quint16 buffer = qToBigEndian((quint16)(Value & 0xffff));
            Dest.append(*((quint8*)&buffer));
            Dest.append(*((quint8*)&buffer + 1));
            break;
        }
        case 0:
        {
            Dest.append((quint8)(Value & 0xff));
        }
    }
}

void TorcBinaryPListSerialiser::CountObjects(quint64 &Count, const QVariant &Value)
{
    switch (static_cast<QMetaType::Type>(Value.type()))
    {
        case QMetaType::QVariantMap:
        {
            QVariantMap map = Value.toMap();
            QVariantMap::const_iterator it = map.begin();
            for ( ; it != map.end(); ++it)
                CountObjects(Count, it.value());
            Count += map.size();
            return;
        }
        case QMetaType::QVariantList:
        {
            Count++;
            bool dict = false;
            QVariantList list = Value.toList();
            QVariantList::const_iterator it = list.begin();
            uint type = list.isEmpty() ? QMetaType::UnknownType : static_cast<QMetaType::Type>(list[0].type());
            for ( ; it != list.end(); ++it)
            {
                if ((*it).type() != type)
                    dict = true;
                CountObjects(Count, (*it));
            }
            if (dict)
                Count += list.size();
            return;
        }
        case (QVariant::Type)QMetaType::QStringList:
        {
            Count++;
            QStringList list = Value.toStringList();
            Count += list.size();
            return;
        }
        default:
            break;
    }

    Count++;
}

class TorcBinaryPListSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcBinaryPListSerialiserFactory() : TorcSerialiserFactory("application", "x-plist", "Binary PList")
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcBinaryPListSerialiser();
    }
} TorcBinaryPListSerialiserFactory;

class TorcAppleBinaryPListSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcAppleBinaryPListSerialiserFactory() : TorcSerialiserFactory("application", "x-apple-binary-plist", "Binary PList")
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcBinaryPListSerialiser();
    }
} TorcAppleBinaryPListSerialiserFactory;

