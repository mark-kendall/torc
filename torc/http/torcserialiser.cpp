/* Class TorcSerialiser
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
#include "torclogging.h"
#include "torcxmlserialiser.h"
#include "torcserialiser.h"

TorcSerialiser* TorcSerialiser::GetSerialiser(const QString &MimeType)
{
    // first create a prioritised list from MimeType
    QStringList rawtypes = MimeType.split(",", QString::SkipEmptyParts);
    QMap<float, QPair<QString,QString>> types;
    foreach (QString rawtype, rawtypes)
    {
        rawtype = rawtype.trimmed();
        int pos = rawtype.indexOf('/');
        if (pos > 0 && pos < (rawtype.size() - 1))
        {
            QString type     = rawtype.left(pos).trimmed();
            QString subtype  = rawtype.mid(pos + 1).trimmed();
            float   priority = 1.0;

            int qpos1 = subtype.indexOf(';');
            int qpos2 = subtype.indexOf('=');
            if (qpos1 > -1 && qpos2 > 0)
            {
                QString prioritys = subtype.mid(qpos2 + 1).trimmed();
                subtype    = subtype.left(qpos1).trimmed();
                bool ok    = false;
                priority   = prioritys.toFloat(&ok);
                priority   = ok ? priority : 0.0;
                // N.B. a fully specified mime type (e.g. text/plain) should have higher priority than partially
                // specified (e.g. text/*)
                if (subtype == "*")
                    priority -= 0.0001; // fudged - this COULD make it lower priority than intended
            }

            types.insertMulti(priority, QPair<QString,QString>(type.toLower(), subtype.toLower()));
        }
    }

    QMapIterator<float, QPair<QString,QString>> it(types);
    it.toBack();
    while (it.hasPrevious())
    {
        it.previous();
        TorcSerialiserFactory *factory = TorcSerialiserFactory::GetTorcSerialiserFactory();
        for ( ; factory; factory = factory->NextTorcSerialiserFactory())
            if (factory->Accepts(it.value()))
                return factory->Create();
    }

    // We don't check for */* - just default to XML as there is no serialiser priority.
    LOG(VB_GENERAL, LOG_WARNING, QString("Failed to find serialiser for '%1' - defaulting to XML").arg(MimeType));
    return new TorcXMLSerialiser();
}

void TorcSerialiser::Serialise(QByteArray &Dest, const QVariant &Data, const QString &Type)
{
    Prepare(Dest);
    Begin(Dest);
    AddProperty(Dest, Type, Data);
    End(Dest);
}

TorcSerialiserFactory* TorcSerialiserFactory::gTorcSerialiserFactory = nullptr;

TorcSerialiserFactory::TorcSerialiserFactory(const QString &Type, const QString &SubType, const QString &Description)
  : m_nextTorcSerialiserFactory(gTorcSerialiserFactory),
    m_type(Type),
    m_subtype(SubType),
    m_description(Description)
{
    gTorcSerialiserFactory      = this;
}

TorcSerialiserFactory* TorcSerialiserFactory::GetTorcSerialiserFactory(void)
{
    return gTorcSerialiserFactory;
}

TorcSerialiserFactory* TorcSerialiserFactory::NextTorcSerialiserFactory(void) const
{
    return m_nextTorcSerialiserFactory;
}

bool  TorcSerialiserFactory::Accepts(const QPair<QString,QString> &MimeType) const
{
    if (m_type == MimeType.first)
    {
        if (m_subtype == MimeType.second)
            return true;
        if ("*" == MimeType.second)
            return true;
    }
    return false;
}

const QString& TorcSerialiserFactory::Description(void) const
{
    return m_description;
}

QString TorcSerialiserFactory::MimeType(void) const
{
    return m_type + "/" + m_subtype;
}
