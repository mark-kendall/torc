/* Class TorcXMLReader
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2015-18
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

// Qt
#include <QFile>

// Torc
#include "torclogging.h"
#include "torcxmlreader.h"

/*! \class TorcXMLReader
 *
 * A simple XML file reader.
 *
 * TorcXMLReader will open and parse an XML file, returning the contents of the file
 * as a hierarchical QVariantMap.
 *
 * There is no support for mixed elements or any attributes - as these cannot be represented
 * within a QVariantMap.
 *
 * \note TorcXMLReader does not support empty elements.
*/
TorcXMLReader::TorcXMLReader(const QString &File)
  : m_reader(new QXmlStreamReader()),
    m_valid(false),
    m_message(QString(""))
{
    if (!QFile::exists(File))
    {
        m_message = QString("File '%1' does not exist").arg(File);
        return;
    }

    QFile file(File);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_message = QString("Cannot open file '%1' for reading").arg(File);
        return;
    }

    m_reader = new QXmlStreamReader(&file);
    m_valid = ReadXML();

    file.close();
}

TorcXMLReader::~TorcXMLReader()
{
    delete m_reader;
}

bool TorcXMLReader::IsValid(QString &Message)
{
    Message = m_message;
    return m_valid;
}

QVariantMultiMap TorcXMLReader::GetResult(void)
{
    return m_map;
}

bool TorcXMLReader::ReadXML(void)
{
    if (!m_reader)
        return false;

    if (!m_reader->readNextStartElement())
        return false;

    QString root = m_reader->name().toString();
    QVariantMultiMap objects;
    while (m_reader->readNextStartElement())
        if (!ReadElement(&objects))
            return false;
    m_map.insert(root, objects);
    return true;
}

/*! \note As noted in the class documentation, there is no support here for mixed elements.
 *        Any text will be overwritten when the child elements are saved.
*/
bool TorcXMLReader::ReadElement(QVariantMultiMap *Map)
{
    if (!m_reader)
        return false;

    QString     name = m_reader->name().toString();
    QVariantMultiMap element;

    bool done = false;
    while (!done)
    {
        switch (m_reader->readNext())
        {
            case QXmlStreamReader::Invalid:
                m_message = QString("XML error: '%1' at line %2:%3").arg(m_reader->errorString()).arg(m_reader->lineNumber()).arg(m_reader->columnNumber());
                return false;
            case QXmlStreamReader::EntityReference:
            case QXmlStreamReader::Characters:
                if (!m_reader->isWhitespace())
                    Map->insert(name, m_reader->text().toString());
                break;
            case QXmlStreamReader::StartElement:
                if (!ReadElement(&element))
                    return false;
                break;
            case QXmlStreamReader::EndDocument:
            case QXmlStreamReader::EndElement:
                done = true;
            case QXmlStreamReader::NoToken:
            case QXmlStreamReader::StartDocument:
            case QXmlStreamReader::ProcessingInstruction:
            case QXmlStreamReader::Comment:
            case QXmlStreamReader::DTD:
                break;
        }
    }

    if (!element.isEmpty())
        Map->insert(name, element);

    return true;
}
