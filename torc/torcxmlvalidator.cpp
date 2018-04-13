/* Class TorcXmlValidator
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

TorcXmlValidator::TorcXmlValidator(const QString &XmlFile, const QString &XSDFile, bool Silent/*=false*/)
  : m_xmlFile(XmlFile),
    m_xsdFile(XSDFile),
    m_xsdData(QByteArray()),
    m_valid(false),
    m_xsdDone(false),
    m_silent(Silent)
{
    Validate();
}

TorcXmlValidator::TorcXmlValidator(const QString &XmlFile, const QByteArray &XSDData, bool Silent/*=false*/)
  : m_xmlFile(XmlFile),
    m_xsdFile(QString("")),
    m_xsdData(XSDData),
    m_valid(false),
    m_xsdDone(false),
    m_silent(Silent)
{
    Validate();
}

TorcXmlValidator::~TorcXmlValidator()
{
}

void TorcXmlValidator::Validate(void)
{
    // sense check xml
    if (!QFile::exists(m_xmlFile))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Xml file '%1' does not exist").arg(m_xmlFile));
        return;
    }

    QFile xml(m_xmlFile);
    if (!xml.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open Xml file '%1'").arg(m_xmlFile));
        return;
    }

    bool xsdvalid = false;
    QXmlSchema schema;
    schema.setMessageHandler(this);

    // sense check xsd
    if (!m_xsdFile.isEmpty())
    {
        if (!QFile::exists(m_xsdFile))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("XSD file '%1' does not exist").arg(m_xsdFile));
            return;
        }

        QFile xsd(m_xsdFile);
        if (!xsd.open(QIODevice::ReadOnly))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open XSD file '%1'").arg(m_xsdFile));
            return;
        }

        xsdvalid = schema.load(&xsd);
        xsd.close();
    }
    else if (!m_xsdData.isEmpty())
    {
        xsdvalid = schema.load(m_xsdData);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "No XSD file or data specified");
    }

    if (!xsdvalid)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("XSD schema '%1' is not valid").arg(m_xsdFile));
        return;
    }

    m_xsdDone = true;

    QXmlSchemaValidator validator(schema);
    validator.setMessageHandler(this);
    if (!validator.validate(&xml))
    {
        if (!m_silent)
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to validate Xml from '%1'").arg(m_xmlFile));
        return;
    }

    m_valid = true;
}

bool TorcXmlValidator::Validated(void)
{
    return m_valid;
}

void TorcXmlValidator::handleMessage(QtMsgType Type, const QString &Description,
                                     const QUrl &Identifier, const QSourceLocation &SourceLocation)
{
    if (m_silent)
        return;

    (void)Identifier;
    QString desc = Description;
    desc.remove(QRegExp("<[^>]*>"));
    if (!m_xsdDone && m_xsdFile.isEmpty())
    {
        LOG(VB_GENERAL, (Type == QtFatalMsg) ? LOG_ERR : LOG_WARNING, QString("Error in XSD at line %1: '%2'")
            .arg(SourceLocation.line()).arg(desc));
    }
    else
    {
        QString file = m_xsdDone ? m_xsdFile : m_xmlFile;
        LOG(VB_GENERAL, (Type == QtFatalMsg) ? LOG_ERR : LOG_WARNING, QString("Error in file '%1' at line %2: '%3'")
            .arg(file).arg(SourceLocation.line()).arg(desc));
    }
}

