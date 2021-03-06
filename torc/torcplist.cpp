/* Class TorcPList
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
#include <QtEndian>
#include <QDateTime>
#include <QTextStream>
#include <QTextCodec>
#include <QBuffer>
#include <QUuid>

// Torc
#include "torclogging.h"
#include "torcplist.h"

#define MAGIC   QByteArray("bplist")
#define VERSION QByteArray("00")
#define MAGIC_SIZE   6
#define VERSION_SIZE 2
#define TRAILER_SIZE 26
#define MIN_SIZE (MAGIC_SIZE + VERSION_SIZE + TRAILER_SIZE)
#define TRAILER_OFFSIZE_INDEX  0
#define TRAILER_PARMSIZE_INDEX 1
#define TRAILER_NUMOBJ_INDEX   2
#define TRAILER_ROOTOBJ_INDEX  10
#define TRAILER_OFFTAB_INDEX   18

// NB Do not call this twice on the same data
static void convert_float(quint8 *p, quint8 s)
{
#if Q_BYTE_ORDER == Q_BIG_ENDIAN && !defined (__VFP_FP__)
    return;
#else
    for (quint8 i = 0; i < (s / 2); i++)
    {
        quint8 t = p[i];
        quint8 j = ((s - 1) + 0) - i;
        p[i] = p[j];
        p[j] = t;
    }
#endif
}

/*! \class TorcPList
 *  \brief A parser for binary property lists
 *
 *  TorcPList uses QVariant for internal storage. Values can be queried using GetValue
 *  and the structure can be exported to Xml with ToXml().
 */
TorcPList::TorcPList(const QByteArray &Data)
  : m_result(),
    m_data(nullptr),
    m_offsetTable(nullptr),
    m_rootObj(0),
    m_numObjs(0),
    m_offsetSize(0),
    m_parmSize(0),
    m_codec(QTextCodec::codecForName("UTF-16BE"))
{
    ParseBinaryPList(Data);
}

///brief Return the value for the given Key.
QVariant TorcPList::GetValue(const QString &Key)
{
    if (m_result.type() != QVariant::Map)
        return QVariant();

    QVariantMap map = m_result.toMap();
    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        if (Key == it.key())
            return it.value();
    }
    return QVariant();
}

///brief Return the complete plist in formatted XML.
QString TorcPList::ToString(void)
{
    QByteArray res;
    QBuffer buf(&res);
    buf.open(QBuffer::WriteOnly);
    if (!this->ToXML(&buf))
        return QStringLiteral("");
    return QString::fromUtf8(res.data());
}

///brief Convert the parsed plist to XML.
bool TorcPList::ToXML(QIODevice *Device)
{
    QXmlStreamWriter XML(Device);
    XML.setAutoFormatting(true);
    XML.setAutoFormattingIndent(4);
    XML.writeStartDocument();
    XML.writeDTD(QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
    XML.writeStartElement(QStringLiteral("plist"));
    XML.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    bool success = ToXML(m_result, XML);
    XML.writeEndElement();
    XML.writeEndDocument();
    if (!success)
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Invalid result."));
    return success;
}

///brief Convert the given parsed Data element to valid XML.
bool TorcPList::ToXML(const QVariant &Data, QXmlStreamWriter &XML)
{
    switch (static_cast<QMetaType::Type>(Data.type()))
    {
        case QMetaType::QVariantMap:
            DictToXML(Data, XML);
            break;
        case QMetaType::QVariantList:
            ArrayToXML(Data, XML);
            break;
        case QMetaType::Int:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::Float:
        case QMetaType::Double:
            XML.writeTextElement(QStringLiteral("real"), QStringLiteral("%1").arg(Data.toDouble(), 0, 'f', 6));
            break;
        case QMetaType::QUuid:
            XML.writeTextElement(QStringLiteral("uid"), Data.toByteArray().toHex().data());
            break;
        case QMetaType::QByteArray:
            XML.writeTextElement(QStringLiteral("data"), Data.toByteArray().toBase64().data());
            break;
        case QMetaType::UInt:
        case QMetaType::UShort:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
            XML.writeTextElement(QStringLiteral("integer"), QStringLiteral("%1").arg(Data.toULongLong()));
            break;
        case QMetaType::QString: XML.writeTextElement(QStringLiteral("string"), Data.toString());
            break;
        case QMetaType::QDateTime:
            XML.writeTextElement(QStringLiteral("date"), Data.toDateTime().toString(Qt::ISODate));
            break;
        case QMetaType::Bool:
            {
                bool val = Data.toBool();
                XML.writeEmptyElement(val ? QStringLiteral("true") : QStringLiteral("false"));
            }
            break;
        case QMetaType::Char:
            XML.writeEmptyElement(QStringLiteral("fill"));
            break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Unknown type."));
            return false;
    }
    return true;
}

///brief Convert the given dictionary data element to valid XML.
void TorcPList::DictToXML(const QVariant &Data, QXmlStreamWriter &XML)
{
    XML.writeStartElement(QStringLiteral("dict"));

    QVariantMap map = Data.toMap();
    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        XML.writeTextElement(QStringLiteral("key"), it.key());
        ToXML(it.value(), XML);
    }

    XML.writeEndElement();
}

///brief Convert the given array data element to valid XML.
void TorcPList::ArrayToXML(const QVariant &Data, QXmlStreamWriter &XML)
{
    XML.writeStartElement(QStringLiteral("array"));

    QList<QVariant> list = Data.toList();
    foreach (const QVariant &item, list)
        ToXML(item, XML);

    XML.writeEndElement();
}

///brief Parse the binary plist contained in Data.
void TorcPList::ParseBinaryPList(const QByteArray &Data)
{
    // reset
    m_result = QVariant();

    // check minimum size
    quint32 size = Data.size();
    if (size < MIN_SIZE)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Binary: size %1, startswith '%2'")
        .arg(size).arg(Data.left(8).data()));

    // check plist type & version
    if ((Data.left(6) != MAGIC) ||
        (Data.mid(MAGIC_SIZE, VERSION_SIZE) != VERSION))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unrecognised start sequence. Corrupt?"));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Parsing binary plist (%1 bytes)")
        .arg(size));

    m_data = (quint8*)Data.data();
    quint8* trailer = m_data + size - TRAILER_SIZE;
    m_offsetSize   = *(trailer + TRAILER_OFFSIZE_INDEX);
    m_parmSize = *(trailer + TRAILER_PARMSIZE_INDEX);
    m_numObjs  = qToBigEndian(*((quint64*)(trailer + TRAILER_NUMOBJ_INDEX)));
    m_rootObj  = qToBigEndian(*((quint64*)(trailer + TRAILER_ROOTOBJ_INDEX)));
    quint64 offset_tindex = qToBigEndian(*((quint64*)(trailer + TRAILER_OFFTAB_INDEX)));
    m_offsetTable = m_data + offset_tindex;

    LOG(VB_GENERAL, LOG_DEBUG,
        QStringLiteral("numObjs: %1 parmSize: %2 offsetSize: %3 rootObj: %4 offsetindex: %5")
            .arg(m_numObjs).arg(m_parmSize).arg(m_offsetSize).arg(m_rootObj).arg(offset_tindex));

    // something wrong?
    if (!m_numObjs || !m_parmSize || !m_offsetSize)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error parsing binary plist. Corrupt?"));
        return;
    }

    // parse
    m_result = ParseBinaryNode(m_rootObj);

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Parse complete."));
}

QVariant TorcPList::ParseBinaryNode(quint64 Num)
{
    quint8* data = GetBinaryObject(Num);
    if (!data)
        return QVariant();

    quint16 type = (*data) & BPLIST_HIGH;
    quint64 size = (*data) & BPLIST_LOW;

    switch (type)
    {
        case BPLIST_SET:
        case BPLIST_ARRAY:   return ParseBinaryArray(data);
        case BPLIST_DICT:    return ParseBinaryDict(data);
        case BPLIST_STRING:  return ParseBinaryString(data);
        case BPLIST_UINT:    return ParseBinaryUInt(&data);
        case BPLIST_REAL:    return ParseBinaryReal(data);
        case BPLIST_DATE:    return ParseBinaryDate(data);
        case BPLIST_DATA:    return ParseBinaryData(data);
        case BPLIST_UNICODE: return ParseBinaryUnicode(data);
        case BPLIST_UID:     return ParseBinaryUID(data);
        case BPLIST_FILL:    return QVariant(QChar());
        case BPLIST_NULL:
        {
            switch (size)
            {
                case BPLIST_TRUE:  return QVariant(true);
                case BPLIST_FALSE: return QVariant(false);
                case BPLIST_NULL:  return QVariant();
            }
        }
    }

    return QVariant();
}

quint64 TorcPList::GetBinaryUInt(quint8 *Data, quint64 Size)
{
    if (Size == 1) return (quint64)(*(Data));
    if (Size == 2) return (quint64)(qToBigEndian(*((quint16*)Data)));
    if (Size == 4) return (quint64)(qToBigEndian(*((quint32*)Data)));
    if (Size == 8) return (quint64)(qToBigEndian(*((quint64*)Data)));

    if (Size == 3)
    {
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
        return (quint64)(((*Data) << 16) + (*(Data + 1) << 8) + (*(Data + 2)));
#else
        return (quint64)((*Data) + (*(Data + 1) << 8) + ((*(Data + 2)) << 16));
#endif
    }

    return 0;
}

quint8* TorcPList::GetBinaryObject(quint64 Num)
{
    if (Num > m_numObjs)
        return nullptr;

    quint8* p = m_offsetTable + (Num * m_offsetSize);
    quint64 offset = GetBinaryUInt(p, m_offsetSize);
    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("GetBinaryObject Num %1, offsize %2 offset %3")
        .arg(Num).arg(m_offsetSize).arg(offset));

    return m_data + offset;
}

QVariantMap TorcPList::ParseBinaryDict(quint8 *Data)
{
    QVariantMap result;
    if (((*Data) & BPLIST_HIGH) != BPLIST_DICT)
        return result;

    quint64 count = GetBinaryCount(&Data);

    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Dict: Size %1").arg(count));

    if (!count)
        return result;

    quint64 off = m_parmSize * count;
    for (quint64 i = 0; i < count; i++, Data += m_parmSize)
    {
        quint64 keyobj = GetBinaryUInt(Data, m_parmSize);
        quint64 valobj = GetBinaryUInt(Data + off, m_parmSize);
        QVariant key = ParseBinaryNode(keyobj);
        QVariant val = ParseBinaryNode(valobj);
        if (!key.canConvert<QString>())
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Invalid dictionary key type '%1' (key %2)")
                .arg(key.type()).arg(keyobj));
            return result;
        }

        result.insertMulti(key.toString(), val);
    }

    return result;
}

QList<QVariant> TorcPList::ParseBinaryArray(quint8 *Data)
{
    QList<QVariant> result;
    if (((*Data) & BPLIST_HIGH) != BPLIST_ARRAY)
        return result;

    quint64 count = GetBinaryCount(&Data);

    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Array: Size %1").arg(count));

    if (!count)
        return result;

    result.reserve(count);
    for (quint64 i = 0; i < count; i++, Data += m_parmSize)
    {
        quint64 obj = GetBinaryUInt(Data, m_parmSize);
        QVariant val = ParseBinaryNode(obj);
        result.push_back(val);
    }
    return result;
}

QVariant TorcPList::ParseBinaryUInt(quint8 **Data)
{
    quint64 result = 0;
    if (((**Data) & BPLIST_HIGH) != BPLIST_UINT)
        return QVariant(result);

    quint64 size = 1 << ((**Data) & BPLIST_LOW);
    (*Data)++;
    result = GetBinaryUInt(*Data, size);
    (*Data) += size;

    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("UInt: %1").arg(result));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryUID(quint8 *Data)
{
    if (((*Data) & BPLIST_HIGH) != BPLIST_UID)
        return QVariant();

    quint64 count = ((*Data) & BPLIST_LOW) + 1; // nnnn+1 bytes
    (*Data)++;

    // these are in reality limited to a bigendian 64bit uint - with 1,2,4 or 8 bytes.
    quint64 uid = GetBinaryUInt(Data, count);
    // which we pack into a QUuid to identify it
    ushort b1 = (uid & 0xff00000000000000) >> 56;
    ushort b2 = (uid & 0x00ff000000000000) >> 48;
    ushort b3 = (uid & 0x0000ff0000000000) >> 40;
    ushort b4 = (uid & 0x000000ff00000000) >> 32;
    ushort b5 = (uid & 0x00000000ff000000) >> 24;
    ushort b6 = (uid & 0x0000000000ff0000) >> 16;
    ushort b7 = (uid & 0x000000000000ff00) >> 8;
    ushort b8 = (uid & 0x00000000000000ff) >> 0;
    return QVariant(QUuid(0, 0, 0, b1, b2, b3, b4, b5, b6, b7, b8));
}

QVariant TorcPList::ParseBinaryString(quint8 *Data)
{
    QString result;
    if (((*Data) & BPLIST_HIGH) != BPLIST_STRING)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    result = QString::fromLatin1((const char*)Data, count);
    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("ASCII String: %1").arg(result));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryReal(quint8 *Data)
{
    double result = 0.0;
    if (((*Data) & BPLIST_HIGH) != BPLIST_REAL)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    count = (quint64)((quint64)1 << count);
    if (count == sizeof(float))
    {
        convert_float(Data, count);
        result = (double)(*((float*)Data));
    }
    else if (count == sizeof(double))
    {
        convert_float(Data, count);
        result = *((double*)Data);
    }

    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Real: %1").arg(result, 0, 'f', 6));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryDate(quint8 *Data)
{
    QDateTime result;
    if (((*Data) & BPLIST_HIGH) != BPLIST_DATE)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (count != 3)
        return result;

    convert_float(Data, 8);
    result = QDateTime::fromTime_t((quint64)(*((double*)Data)));
    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Date: %1").arg(result.toString(Qt::ISODate)));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryData(quint8 *Data)
{
    QByteArray result;
    if (((*Data) & BPLIST_HIGH) != BPLIST_DATA)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    result = QByteArray((const char*)Data, count);
    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Data: Size %1 (count %2)")
        .arg(result.size()).arg(count));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryUnicode(quint8 *Data)
{
    QString result;
    if (((*Data) & BPLIST_HIGH) != BPLIST_UNICODE)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    return QVariant(m_codec->toUnicode((char*)Data, count << 1));
}

quint64 TorcPList::GetBinaryCount(quint8 **Data)
{
    quint64 count = (**Data) & BPLIST_LOW;
    (*Data)++;
    if (count == BPLIST_LOW_MAX)
    {
        QVariant newcount = ParseBinaryUInt(Data);
        if (!newcount.canConvert<quint64>())
            return 0;
        count = newcount.toULongLong();
    }
    return count;
}
