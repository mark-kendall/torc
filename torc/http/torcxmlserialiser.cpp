/* Class TorcXMLSerialiser
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torcxmlserialiser.h"

/*! \class TorcXMLSerialiser
 *  \brief A serialiser for XML formatted output.
 *
 * TorcXMLSerialiser uses QXmlStreamWriter for the bulk of the serialisation overhead.
 *
 * \todo Ensure complete consistency and interopability with all other serialisers.
*/
TorcXMLSerialiser::TorcXMLSerialiser()
  : TorcSerialiser(),
    m_xmlStream(),
    m_buffer()
{
}

TorcXMLSerialiser::~TorcXMLSerialiser()
{
}

HTTPResponseType TorcXMLSerialiser::ResponseType(void)
{
    return HTTPResponseXML;
}

void TorcXMLSerialiser::Prepare(QByteArray &Dest)
{
    m_buffer.setBuffer(&Dest);
    m_buffer.open(QBuffer::ReadWrite);
    m_xmlStream.setDevice(&m_buffer);
}

void TorcXMLSerialiser::Begin(QByteArray &Dest)
{
    (void)Dest;
    m_xmlStream.writeStartDocument("1.0");
}

void TorcXMLSerialiser::End(QByteArray &Dest)
{
    (void)Dest;
    m_xmlStream.writeEndDocument();
}

void TorcXMLSerialiser::AddProperty(QByteArray &Dest, const QString &Name, const QVariant &Value)
{
    (void)Dest;
    if (!Name.isEmpty())
        m_xmlStream.writeStartElement(Name);
    VariantToXML(Name, Value);
    if (!Name.isEmpty())
        m_xmlStream.writeEndElement();
}

void TorcXMLSerialiser::VariantToXML(const QString &Name, const QVariant &Value)
{
    switch(static_cast<QMetaType::Type>(Value.type()))
    {
        case QMetaType::QVariantList: ListToXML(Name, Value.toList());             return;
        case QMetaType::QStringList:  StringListToXML(Name, Value.toStringList()); return;
        case QMetaType::QVariantMap:  MapToXML(Name, Value.toMap());               return;
        case (QVariant::Type)QMetaType::QDateTime:
        {
            QDateTime datetime(Value.toDateTime());
            if (datetime.isNull())
                m_xmlStream.writeAttribute("xsi:nil", "true");
            m_xmlStream.writeCharacters(datetime.toString(Qt::ISODate));
            return;
        }
        default: m_xmlStream.writeCharacters(Value.toString()); return;
    }
}

void TorcXMLSerialiser::ListToXML(const QString &Name, const QVariantList &Value)
{
    if (!Value.isEmpty())
    {
        int type = Value[0].type();
        QVariantList::const_iterator it = Value.begin();
        for ( ; it != Value.end(); ++it)
        {
            if ((int)(*it).type() != type)
            {
                VariantToXML("Error", QVARIANT_ERROR);
                return;
            }
        }
    }

    QVariantList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream.writeStartElement(Name);
        VariantToXML(Name, (*it));
        m_xmlStream.writeEndElement();
    }
}

void TorcXMLSerialiser::StringListToXML(const QString &Name, const QStringList &Value)
{
    (void)Name;

    QStringList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream.writeStartElement("String");
        m_xmlStream.writeCharacters((*it));
        m_xmlStream.writeEndElement();
    }
}

void TorcXMLSerialiser::MapToXML(const QString &Name, const QVariantMap &Value)
{
    QVariantMap::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream.writeStartElement(it.key());
        VariantToXML(Name, it.value());
        m_xmlStream.writeEndElement();
    }
}

class TorcXMLSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcXMLSerialiserFactory() : TorcSerialiserFactory("application", "xml", "XML")
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcXMLSerialiser();
    }
} TorcXMLSerialiserFactory;
