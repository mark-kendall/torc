/* Class TorcPlainTextSerialiser
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
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

#include "torcplaintextserialiser.h"

TorcPlainTextSerialiser::TorcPlainTextSerialiser() : TorcSerialiser()
{
}

HTTPResponseType TorcPlainTextSerialiser::ResponseType(void)
{
    return HTTPResponsePlainText;
}

void TorcPlainTextSerialiser::Prepare(QByteArray &)
{
}

void TorcPlainTextSerialiser::Begin(QByteArray &)
{
}

void TorcPlainTextSerialiser::End(QByteArray &)
{
}

void TorcPlainTextSerialiser::AddProperty(QByteArray &Dest, const QString &Name, const QVariant &Value)
{
    // Name is added for consistency with other serialisers...
    Dest = QByteArray(Name.toLocal8Bit() + "\r\n" + Value.toByteArray());
}

class TorcPlainTextSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcPlainTextSerialiserFactory() : TorcSerialiserFactory(QStringLiteral("text"), QStringLiteral("plain"), QStringLiteral("Text"))
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcPlainTextSerialiser();
    }
} TorcPlainTextSerialiserFactory;
