/* Class TorcPListSerialiser
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

// Torc
#include "torcplistserialiser.h"

TorcPListSerialiser::TorcPListSerialiser()
  : TorcXMLSerialiser()
{
}

HTTPResponseType TorcPListSerialiser::ResponseType(void)
{
    return HTTPResponsePList;
}

void TorcPListSerialiser::Begin(QByteArray &)
{
    m_xmlStream.setAutoFormatting(true);
    m_xmlStream.setAutoFormattingIndent(4);
    m_xmlStream.writeStartDocument(QStringLiteral("1.0"));
    m_xmlStream.writeDTD(QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
    m_xmlStream.writeStartElement(QStringLiteral("plist"));
    m_xmlStream.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    m_xmlStream.writeStartElement(QStringLiteral("dict"));
}

void TorcPListSerialiser::AddProperty(QByteArray &, const QString &Name, const QVariant &Value)
{
    PListFromVariant(Name, Value);
}

void TorcPListSerialiser::End(QByteArray &)
{
    m_xmlStream.writeEndElement();
    m_xmlStream.writeEndElement();
    m_xmlStream.writeEndDocument();
}

void TorcPListSerialiser::PListFromVariant(const QString &Name, const QVariant &Value, bool NeedKey)
{
    if (Value.isNull())
    {
        if (NeedKey)
            m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
        m_xmlStream.writeEmptyElement(QStringLiteral("null"));
        return;
    }

    switch ((int)Value.type())
    {
        case QMetaType::QVariantList: PListFromList(Name, Value.toList());             break;
        case QMetaType::QStringList:  PListFromStringList(Name, Value.toStringList()); break;
        case QMetaType::QVariantMap:  PListFromMap(Name, Value.toMap());               break;
        case QMetaType::QDateTime:
        {
            if (Value.toDateTime().isValid())
            {
                if (NeedKey)
                    m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
                m_xmlStream.writeTextElement(QStringLiteral("date"), Value.toDateTime().toUTC().toString(QStringLiteral("yyyy-MM-ddThh:mm:ssZ")));
            }
            break;
        }
        case QMetaType::QByteArray:
        {
            if (!Value.toByteArray().isNull())
            {
                if (NeedKey)
                    m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
                m_xmlStream.writeTextElement(QStringLiteral("data"), Value.toByteArray().toBase64().data());
            }
            break;
        }
        case QMetaType::Bool:
        {
            if (NeedKey)
                m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
            m_xmlStream.writeEmptyElement(Value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
            break;
        }
        case QMetaType::Char:
        {
            if (NeedKey)
                m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
            m_xmlStream.writeEmptyElement(QStringLiteral("fill"));
            break;
        }
        break;
        case QMetaType::UInt:
        case QMetaType::UShort:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
        {
            if (NeedKey)
                m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
            m_xmlStream.writeTextElement(QStringLiteral("integer"), QString::number(Value.toULongLong()));
            break;
        }

        case QMetaType::Int:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::Float:
        case QMetaType::Double:
        {
            if (NeedKey)
                m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
            m_xmlStream.writeTextElement(QStringLiteral("real"), QStringLiteral("%1").arg(Value.toDouble(), 0, 'f', 6));
            break;
        }

        case QMetaType::QString:
        default:
        {
            if (NeedKey)
                m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
            m_xmlStream.writeTextElement(QStringLiteral("string"), Value.toString());
            break;
        }
    }
}

void TorcPListSerialiser::PListFromList(const QString &Name, const QVariantList &Value)
{
    if (!Value.isEmpty())
    {
        int type = Value[0].type();

        QVariantList::const_iterator it = Value.begin();
        for ( ; it != Value.end(); ++it)
        {
            if ((int)(*it).type() != type)
            {
                PListFromVariant(QStringLiteral("Error"), QVARIANT_ERROR);
                return;
            }
        }
    }

    m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
    m_xmlStream.writeStartElement(QStringLiteral("array"));

    QVariantList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        PListFromVariant(Name, (*it), false);

    m_xmlStream.writeEndElement();
}

void TorcPListSerialiser::PListFromStringList(const QString &Name, const QStringList &Value)
{
    m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
    m_xmlStream.writeStartElement(QStringLiteral("array"));

    QStringList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        m_xmlStream.writeTextElement(QStringLiteral("string"), (*it));

    m_xmlStream.writeEndElement();
}

void TorcPListSerialiser::PListFromMap(const QString &Name, const QVariantMap &Value)
{
    m_xmlStream.writeTextElement(QStringLiteral("key"), Name);
    m_xmlStream.writeStartElement(QStringLiteral("dict"));

    QVariantMap::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        PListFromVariant(it.key(), it.value());

    m_xmlStream.writeEndElement();
}

class TorcApplePListSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcApplePListSerialiserFactory() : TorcSerialiserFactory(QStringLiteral("text"), QStringLiteral("x-apple-plist+xml"), QStringLiteral("PList"))
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcPListSerialiser();
    }
} TorcApplePListSerialiserFactory;

class TorcPListSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcPListSerialiserFactory() : TorcSerialiserFactory(QStringLiteral("application"), QStringLiteral("plist"), QStringLiteral("PList"))
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcPListSerialiser();
    }
} TorcPListSerialiserFactory;


