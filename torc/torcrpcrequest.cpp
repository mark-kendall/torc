/* Class TorcRPCRequest
*
* Copyright (C) Mark Kendall 2013-18
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
#include <QJsonArray>
#include <QJsonDocument>

// Torc
#include "torclogging.h"
#include "http/torchttpserver.h"
#include "torcrpcrequest.h"

/*! \class TorcRPCRequest
 *  \brief A class encapsulating a Remote Procedure Call.
 *
 * Remote Procedure Calls are currently only handled through TorcWebSocket.
 *
 * \note The underlying protocol is restricted to JSON-RPC.
 *
 * \sa TorcWebSocket
 * \sa TorcHTTPService
*/

/*! \brief Creates an RPC request owned by the given Parent.
 *
 * Parent will be notified when the request has been completed (or on error) and is
 * responsible for cancelling the request if it is no longer required. Upon completion,
 * error or cancellation, the parent must then DownRef the request to ensure it is
 * deleted.
*/
TorcRPCRequest::TorcRPCRequest(const QString &Method, QObject *Parent)
  : m_authenticated(false),
    m_notification(false),
    m_state(None),
    m_id(-1),
    m_method(Method),
    m_parent(NULL),
    m_parentLock(new QMutex()),
    m_validParent(true),
    m_parameters(),
    m_positionalParameters(),
    m_serialisedData(),
    m_reply()
{
    SetParent(Parent);
}

/*! \brief Creates a notification request for which no response is expected.
 *
 * Notification requests are deleted after they have been sent.
*/
TorcRPCRequest::TorcRPCRequest(const QString &Method)
  : m_authenticated(false),
    m_notification(true),
    m_state(None),
    m_id(-1),
    m_method(Method),
    m_parent(NULL),
    m_parentLock(new QMutex()),
    m_validParent(false),
    m_parameters(),
    m_positionalParameters(),
    m_serialisedData(),
    m_reply()
{
}

/*! \brief Creates a request from the given QJsonObject
*/
TorcRPCRequest::TorcRPCRequest(const QJsonObject &Object, QObject *Parent, bool Authenticated)
  : m_authenticated(Authenticated),
    m_notification(true),
    m_state(None),
    m_id(-1),
    m_method(),
    m_parent(Parent),
    m_parentLock(new QMutex()),
    m_validParent(false),
    m_parameters(),
    m_positionalParameters(),
    m_serialisedData(),
    m_reply()
{
    ParseJSONObject(Object);
}

/*! \brief Creates a request or response from the given raw data using the given protocol.
 *
*/
TorcRPCRequest::TorcRPCRequest(TorcWebSocketReader::WSSubProtocol Protocol, const QByteArray &Data, QObject *Parent, bool Authenticated)
  : m_authenticated(Authenticated),
    m_notification(true),
    m_state(None),
    m_id(-1),
    m_method(),
    m_parent(Parent),
    m_parentLock(new QMutex()),
    m_validParent(false),
    m_parameters(),
    m_positionalParameters(),
    m_serialisedData(),
    m_reply()
{
    if (Protocol != TorcWebSocketReader::SubProtocolJSONRPC)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown websocket subprotocol");
        return;
    }

    // parse the JSON
    QJsonDocument doc = QJsonDocument::fromJson(Data);

    if (doc.isNull())
    {
        ProcessNullContent(Data.contains("method"));
        return;
    }

    LOG(VB_GENERAL, LOG_DEBUG, Data);

    // single request, one JSON object
    if (doc.isObject())
    {
        ParseJSONObject(doc.object());
        return;
    }
    // batch call
    else if (doc.isArray())
    {
        ProcessBatchCall(doc.array());
        return;
    }
}

TorcRPCRequest::~TorcRPCRequest()
{
    delete m_parentLock;
}

void TorcRPCRequest::ProcessBatchCall(const QJsonArray &Array)
{
    QJsonObject error;
    QJsonObject object;
    object.insert("code",    -32600);
    object.insert("message", QString("Invalid request"));
    error.insert("error",    object);
    error.insert("jsonrpc",  QString("2.0"));
    error.insert("id",       QJsonValue());

    // an empty array is an error
    if (Array.isEmpty())
    {
        m_serialisedData = QJsonDocument(error).toJson();
        LOG(VB_GENERAL, LOG_ERR, "Invalid request - empty array");
        return;
    }

    // iterate over each member
    QByteArray result;
    result.append("[\r\n");
    bool empty = true;

    QJsonArray::const_iterator it = Array.constBegin();
    for ( ; it != Array.constEnd(); ++it)
    {
        // must be an object
        if (!(*it).isObject())
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid request - not an object");
            result.append(QJsonDocument(error).toJson());
            continue;
        }

        // process this object
        TorcRPCRequest *request = new TorcRPCRequest((*it).toObject(), m_parent, m_authenticated);

        if (!request->GetData().isEmpty())
        {
            if (empty)
                empty = false;
            else
                result.append(",\r\n");
            result.append(request->GetData());
        }

        request->DownRef();
    }

    result.append("\r\n]");

    // don't return an empty array - which would/should be a group of notifications...
    if (!empty)
        m_serialisedData = result;
}

void TorcRPCRequest::ProcessNullContent(bool HasMethod)
{
    // NB we are acting as both client and server, hence if we receive invalid JSON (that Qt cannot parse)
    // we can only make a best efforts guess as to whether this was a request. Hence under
    // certain circumstances, we may not send the appropriate error message or respond at all.
    if (HasMethod)
    {
        QJsonObject object;
        QJsonObject error;
        error.insert("code", -32700);
        error.insert("message", QString("Parse error"));
        object.insert("error",   error);
        object.insert("jsonrpc", QString("2.0"));
        object.insert("id",      QJsonValue());
        m_serialisedData = QJsonDocument(object).toJson();
        LOG(VB_GENERAL, LOG_INFO, m_serialisedData);
    }

    LOG(VB_GENERAL, LOG_ERR, "Error parsing JSON-RPC data");
    AddState(Errored);
}

void TorcRPCRequest::ParseJSONObject(const QJsonObject &Object)
{
    // determine whether this is a request or response
    int  id        = (Object.contains("id") && !Object["id"].isNull()) ? (int)Object["id"].toDouble() : -1;
    bool isrequest = Object.contains("method");
    bool isresult  = Object.contains("result");
    bool iserror   = Object.contains("error");

    if ((int)isrequest + (int)isresult + (int)iserror != 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "Ambiguous RPC request/response");
        AddState(Errored);
        return;
    }

    if (isrequest)
    {
        QString method = Object["method"].toString();
        // if this is a notification, check first whether it is a subscription 'event' that the parent is monitoring
        bool handled = false;
        if (id < 0)
        {
            if (!QMetaObject::invokeMethod(m_parent, "HandleNotification", Qt::DirectConnection,
                                           Q_RETURN_ARG(bool, handled), Q_ARG(QString, method)))
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to invoke 'HandleNotification' in request parent");
            }
        }

        if (!handled)
        {
            QVariantMap result = TorcHTTPServer::HandleRequest(method, Object.value("params").toVariant(), m_parent, m_authenticated);

            // not a notification, response expected
            if (id > -1)
            {
                // result should contain either 'result' or 'error', we need to insert id and protocol identifier
                result.insert("jsonrpc", QString("2.0"));
                result.insert("id", id);
                m_serialisedData = QJsonDocument::fromVariant(result).toJson();
            }
            else if (Object.contains("id"))
            {
                LOG(VB_GENERAL, LOG_ERR, "Request contains invalid id");
            }
        }
    }
    else if (isresult)
    {
        m_reply = Object.value("result").toVariant();
        AddState(Result);
        m_id = id;

        if (m_id < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Received result with no id");
            AddState(Errored);
        }
    }
    else if (iserror)
    {
        AddState(Errored);
        AddState(Result);
        m_id = id;

        if (m_id < 0)
            LOG(VB_GENERAL, LOG_ERR, "Received error with no id");
        else
            LOG(VB_GENERAL, LOG_ERR, "JSON-RPC error");
    }
}

bool TorcRPCRequest::IsNotification(void) const
{
    return m_notification;
}

///\brief Signal to the parent that the request is ready (but may be errored).
void TorcRPCRequest::NotifyParent(void)
{
    QMutexLocker locker(m_parentLock);

    if (!m_parent || !m_validParent || m_state & Cancelled)
        return;

    QMetaObject::invokeMethod(m_parent, "RequestReady", Q_ARG(TorcRPCRequest*, this));
}

/*! \brief Set the parent for the request
 *
 * Usually only used when a request is being cancelled by the parent and the parent
 * is being deleted.
*/
void TorcRPCRequest::SetParent(QObject *Parent)
{
    QMutexLocker locker(m_parentLock);

    m_parent = Parent;
    m_validParent = m_parent != NULL;

    if (m_parent && m_parent->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("RequestReady(TorcRPCRequest*)")) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Request's parent does not have RequestReady method - request WILL fail");
        m_validParent = false;
    }
}

/*! \brief Serialise the request for the given protocol.
 *
 * \note QJsonValue may not be flexbible enough for all types.
*/
QByteArray& TorcRPCRequest::SerialiseRequest(TorcWebSocketReader::WSSubProtocol Protocol)
{
    if (Protocol == TorcWebSocketReader::SubProtocolJSONRPC)
    {
        QJsonObject object;
        object.insert("jsonrpc", QString("2.0"));
        object.insert("method",  m_method);

        // named paramaters are preferred over positional
        if (!m_parameters.isEmpty())
        {
            QJsonObject params;
            for (int i = 0; i < m_parameters.size(); ++i)
                params.insert(m_parameters[i].first, QJsonValue::fromVariant(m_parameters[i].second));
            object.insert("params", params);
        }
        else if (!m_positionalParameters.isEmpty())
        {
            // NB positional parameters are serialised as an array (ordered)
            QJsonArray params;
            for (int i = 0; i < m_positionalParameters.size(); ++i)
                params.append(QJsonValue::fromVariant(m_positionalParameters[i]));
            object.insert("params", params);
        }

        if (m_id > -1)
            object.insert("id", m_id);

        QJsonDocument doc(object);
        m_serialisedData = doc.toJson();
    }

    LOG(VB_NETWORK, LOG_DEBUG, m_serialisedData);

    return m_serialisedData;
}

/*! \brief Progress the state for this request.
 *
 * \note Request state is incremental.
*/
void TorcRPCRequest::AddState(int State)
{
    m_state |= State;
}

///\brief Set the unique ID for this request.
void TorcRPCRequest::SetID(int ID)
{
    m_id = ID;
}

/*! \brief Add Parameter and value to list of call parameters.
 *
 * Ideally this should check for duplicate parameter names but it is not a commonly
 * used method, would be inneficient and the caller should control the duplication.
*/
void TorcRPCRequest::AddParameter(const QString &Name, const QVariant &Value)
{
    m_parameters.append(QPair<QString,QVariant>(Name, Value));
}

/*! \brief Add a positional parameter.
 *
 * Positional parameters are unnamed and ordered. They are only added if the normal (preferred)
 * parameter list is empty.
*/
void TorcRPCRequest::AddPositionalParameter(const QVariant &Value)
{
    m_positionalParameters.append(Value);
}

void TorcRPCRequest::SetReply(const QVariant &Reply)
{
    m_reply = Reply;
}

int TorcRPCRequest::GetState(void) const
{
    return m_state;
}

int TorcRPCRequest::GetID(void) const
{
    return m_id;
}

QString TorcRPCRequest::GetMethod(void) const
{
    return m_method;
}

QObject* TorcRPCRequest::GetParent(void) const
{
    return m_parent;
}

const QVariant& TorcRPCRequest::GetReply(void) const
{
    return m_reply;
}

const QList<QPair<QString,QVariant> >& TorcRPCRequest::GetParameters(void) const
{
    return m_parameters;
}

const QList<QVariant>& TorcRPCRequest::GetPositionalParameters(void) const
{
    return m_positionalParameters;
}

QByteArray& TorcRPCRequest::GetData(void)
{
    return m_serialisedData;
}
