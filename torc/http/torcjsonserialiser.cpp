/* Class TorcJSONSerialiser
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

// Qt
#include <QJsonDocument>

// Torc
#include "torcjsonserialiser.h"

TorcJSONSerialiser::TorcJSONSerialiser(bool Javascript)
  : TorcSerialiser(),
    m_javaScriptType(Javascript)
{
}

HTTPResponseType TorcJSONSerialiser::ResponseType(void)
{
    return m_javaScriptType ? HTTPResponseJSONJavascript : HTTPResponseJSON;
}

void TorcJSONSerialiser::Prepare(QByteArray &)
{
}

void TorcJSONSerialiser::Begin(QByteArray &)
{
}

void TorcJSONSerialiser::AddProperty(QByteArray &Dest, const QString &Name, const QVariant &Value)
{
    if (Name.isEmpty())
    {
        Dest = QByteArray(QJsonDocument::fromVariant(Value).toJson(QJsonDocument::Compact));
    }
    else
    {
        QVariantMap map;
        map.insert(Name, Value);
        Dest = QByteArray(QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact));
    }
}

void TorcJSONSerialiser::End(QByteArray &)
{
}

class TorcJSONSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcJSONSerialiserFactory() : TorcSerialiserFactory(QStringLiteral("application"), QStringLiteral("json"), QStringLiteral("JSON"))
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcJSONSerialiser();
    }
} TorcJSONSerialiserFactory;

class TorcJavascriptSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcJavascriptSerialiserFactory() : TorcSerialiserFactory(QStringLiteral("text"), QStringLiteral("javascript"), QStringLiteral("JSON"))
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcJSONSerialiser(true /*javascript*/);
    }
} TorcJavascriptSerialiserFactory;
