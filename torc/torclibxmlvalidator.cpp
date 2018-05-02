/* Class TorcXmlValidator
*
* Copyright (C) Mark Kendall 2016-18
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

// Torc
#include "torclogging.h"
#include "torclibxmlvalidator.h"

// libxml2
#include <libxml/parser.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>

TorcXmlValidator::TorcXmlValidator(const QString &XmlFile, const QString &XSDFile, bool Silent /*= false*/)
  : m_xmlFile(XmlFile),
    m_xsdFile(XSDFile),
    m_xsdData(QByteArray()),
    m_valid(false),
    m_xsdDone(false),
    m_silent(Silent)
{
    Validate();
}

TorcXmlValidator::TorcXmlValidator(const QString &XmlFile, const QByteArray &XSDData, bool Silent /*= false*/)
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

void TorcXmlValidator::HandleMessage(void *ctx, const char *format, ...)
{
    bool silent = false;

    TorcXmlValidator* owner = static_cast<TorcXmlValidator*>(ctx);
    if (owner)
        silent = owner->GetSilent();

    if (silent)
        return;

    char *error;
    va_list args;
    va_start(args, format);
    (void)vasprintf(&error, format, args);
    va_end(args);
    LOG(VB_GENERAL, LOG_ERR, QString("Libxml error: %1").arg(error));
    free(error);
}


void TorcXmlValidator::Validate(void)
{
    xmlSchemaParserCtxtPtr parsercontext = NULL;

    if (!m_xsdFile.isEmpty())
    {
        parsercontext = xmlSchemaNewParserCtxt(m_xsdFile.toLocal8Bit().constData());
    }
    else if (!m_xsdData.isEmpty())
    {
        parsercontext = xmlSchemaNewMemParserCtxt(m_xsdData.constData(), m_xsdData.size());
    }

    if (!parsercontext)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create parser context for XSD");
        return;
    }

    xmlSchemaPtr schema = xmlSchemaParse(parsercontext);

    if (!schema)
    {
        xmlSchemaFreeParserCtxt(parsercontext);
        LOG(VB_GENERAL, LOG_ERR, "Failed to parse XSD");
        return;
    }

    xmlSchemaValidCtxtPtr validcontext = xmlSchemaNewValidCtxt(schema);

    if (!validcontext)
    {
        xmlSchemaFree(schema);
        xmlSchemaFreeParserCtxt(parsercontext);
        LOG(VB_GENERAL, LOG_ERR, "Could not create XSD schema validation context");
        return;
    }

    if (!m_silent)
        LOG(VB_GENERAL, LOG_INFO, "XSD loaded and validated");

    m_xsdDone = true;

    xmlSetStructuredErrorFunc(NULL, NULL);
    xmlSetGenericErrorFunc(this, HandleMessage);
    xmlThrDefSetStructuredErrorFunc(NULL, NULL);
    xmlThrDefSetGenericErrorFunc(this, HandleMessage);

    xmlDocPtr xmldocumentptr = xmlParseFile(m_xmlFile.toLocal8Bit().constData());
    int result = xmlSchemaValidateDoc(validcontext, xmldocumentptr);

    if (result == 0)
        m_valid = true;

    xmlFreeDoc(xmldocumentptr);
    xmlSchemaFreeValidCtxt(validcontext);
    xmlSchemaFree(schema);
    xmlSchemaFreeParserCtxt(parsercontext);
}

bool TorcXmlValidator::GetSilent(void) const
{
    return m_silent;
}

bool TorcXmlValidator::Validated(void) const
{
    return m_valid;
}
