/* Class TorcXmlValidator
*
* Copyright (C) Mark Kendall 2015
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
#include <QFile>
#include <QRegExp>
#include <QXmlSchema>
#include <QXmlSchemaValidator>

// Torc
#include "torclogging.h"
#include "torcxmlvalidator.h"

TorcXmlValidator::TorcXmlValidator(const QString &XmlFile, const QString &XSDFile)
  : m_xmlFile(XmlFile),
    m_xsdFile(XSDFile),
    m_valid(false)
{
    // initial sense checks
    if (!QFile::exists(m_xmlFile))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Xml file '%1' does not exist").arg(m_xmlFile));
        return;
    }

    if (!QFile::exists(m_xsdFile))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("XSD file '%1' does not exist").arg(m_xsdFile));
        return;
    }

    QFile xml(m_xmlFile);
    QFile xsd(m_xsdFile);

    if (!xml.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open Xml file '%1'").arg(m_xmlFile));
        return;
    }

    if (!xsd.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open XSD file '%1'").arg(m_xsdFile));
        return;
    }

    QXmlSchema schema;
    schema.setMessageHandler(this);
    if (!schema.load(&xsd))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("XSD schema '%1' is not valid").arg(m_xsdFile));
        return;
    }

    QXmlSchemaValidator validator(schema);
    validator.setMessageHandler(this);
    if (!validator.validate(&xml))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to validate Xml from '%1'").arg(m_xmlFile));
        return;
    }

    m_valid = true;
}

TorcXmlValidator::~TorcXmlValidator()
{
}

bool TorcXmlValidator::Validated(void)
{
    return m_valid;
}

void TorcXmlValidator::handleMessage(QtMsgType Type, const QString &Description,
                                     const QUrl &Identifier, const QSourceLocation &SourceLocation)
{
    (void)Identifier;
    QString desc = Description;
    desc.remove(QRegExp("<[^>]*>"));
    LOG(VB_GENERAL, Type == QtFatalMsg ? LOG_ERR : LOG_WARNING, QString("XSDError in file '%1' at line %2: '%3'")
        .arg(m_xmlFile).arg(SourceLocation.line()).arg(desc));
}

