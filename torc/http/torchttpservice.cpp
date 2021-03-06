/* Class TorcHTTPService
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
#include <QObject>
#include <QMetaType>
#include <QMetaMethod>
#include <QTime>
#include <QDate>
#include <QDateTime>

// Torc
#include "torclocaldefs.h"
#include "torclogging.h"
#include "torcnetwork.h"
#include "torchttpserver.h"
#include "torcjsonrpc.h"
#include "torcserialiser.h"
#include "torchttpservice.h"
#include "torcexitcodes.h"

class MethodParameters
{
  public:
    MethodParameters(int Index, QMetaMethod Method, int AllowedRequestTypes, const QString &ReturnType)
      : m_valid(false),
        m_index(Index),
        m_names(),
        m_types(),
        m_allowedRequestTypes(AllowedRequestTypes),
        m_returnType(ReturnType),
        m_method(Method)
    {
        // statically initialise the list of unsupported types (either non-serialisable (QHash)
        // or nonsensical (pointer types)
        static QList<int> unsupportedtypes;
        static QList<int> unsupportedparameters;
        static bool initialised = false;

        if (!initialised)
        {
            // this list is probably incomplete
            initialised = true;
            unsupportedtypes << QMetaType::UnknownType;
            unsupportedtypes << QMetaType::VoidStar << QMetaType::QObjectStar << QMetaType::QVariantHash;
            unsupportedtypes << QMetaType::QRect << QMetaType::QRectF << QMetaType::QSize << QMetaType::QSizeF << QMetaType::QLine << QMetaType::QLineF << QMetaType::QPoint << QMetaType::QPointF;

            unsupportedparameters << unsupportedtypes;
            unsupportedparameters << QMetaType::QVariantMap << QMetaType::QStringList << QMetaType::QVariantList;
        }

        // the return type/value is first
        int returntype = QMetaType::type(Method.typeName());

        // discard slots with an unsupported return type
        if (unsupportedtypes.contains(returntype))
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Method '%1' has unsupported return type '%2'").arg(Method.name().constData(), Method.typeName()));
            return;
        }

        // discard overly complicated slots not supported by QMetaMethod
        if (Method.parameterCount() > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Method '%1' takes more than 10 parameters - ignoring").arg(Method.name().constData()));
            return;
        }

        m_types.append(returntype > 0 ? returntype : 0);
        m_names.append(Method.name());

        QList<QByteArray> names = Method.parameterNames();
        QList<QByteArray> types = Method.parameterTypes();

        // add type/value for each method parameter
        for (int i = 0; i < names.size(); ++i)
        {
            int type = QMetaType::type(types[i]);

            // discard slots that use unsupported parameter types
            if (unsupportedparameters.contains(type))
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Method '%1' has unsupported parameter type '%2'")
                    .arg(Method.name().constData(), types[i].constData()));
                return;
            }

            m_names.append(names[i]);
            m_types.append(type);
        }

        m_valid = true;
    }

    ~MethodParameters() = default;

    /*! \brief Call the stored method with the arguments passed in via Queries.
     *
     * \note Invoke is not thread safe and any method exposed using this class MUST ensure thread safety.
    */
    QVariant Invoke(QObject *Object, const QMap<QString,QString> &Queries, QString &ReturnType, bool &VoidResult)
    {
        // this may be called by multiple threads simultaneously, so we need to create our own paramaters instance.
        // N.B. QMetaObject::invokeMethod only supports up to 10 arguments (plus a return value)
        void* parameters[11];
        memset(parameters, 0, 11 * sizeof(void*));
        int size = qMin(11, m_types.size());

        // check parameter count
        if (Queries.size() != size - 1)
        {
            if (!m_names.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Method '%1' expects %2 parameters, sent %3")
                   .arg(m_names.value(0).constData()).arg(size - 1).arg(Queries.size()));
            }
            return QVariant();
        }

        // populate parameters from query and ensure each parameter is listed
        QMap<QString,QString>::const_iterator it;
        for (int i = 0; i < size; ++i)
        {
            parameters[i] = QMetaType::create(m_types.value(i));
            if (i)
            {
                it = Queries.constFind(m_names.value(i));
                if (it == Queries.end())
                {
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Parameter '%1' for method '%2' is missing")
                        .arg(m_names.value(i).constData(), m_names.value(0).constData()));
                    return QVariant();
                }
                SetValue(parameters[i], it.value(), m_types.value(i));
            }
        }

        if (Object->qt_metacall(QMetaObject::InvokeMetaMethod, m_index, parameters) > -1)
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("qt_metacall error"));

        // handle QVariant return type where we don't want to lose visibility of the underlying type
        int type = m_types.value(0);
        if (type == QMetaType::QVariant)
        {
            int newtype = static_cast<int>(reinterpret_cast<QVariant*>(parameters[0])->type());
            if (newtype != type)
                type = newtype;
        }

        // we cannot create a QVariant that is void and an invalid QVariant signals an error state,
        // so flag directly
        VoidResult      = type == QMetaType::Void;
        QVariant result = type == QMetaType::Void ? QVariant() : QVariant(type, parameters[0]);

        // free allocated parameters
        for (int i = 0; i < size; ++i)
            if (parameters[i])
                QMetaType::destroy(m_types.at(i), parameters[i]);

        ReturnType = m_returnType;
        return result;
    }

    static void SetValue(void* Pointer, const QString &Value, int Type)
    {
        if (!Pointer)
            return;

        switch (Type)
        {
            case QMetaType::QVariant:   *((QVariant*)Pointer)      = QVariant(Value); return;
            case QMetaType::Char:       *((char*)Pointer)          = Value.isEmpty() ? 0 : Value.at(0).toLatin1(); return;
            case QMetaType::UChar:      *((unsigned char*)Pointer) = Value.isEmpty() ? 0 :Value.at(0).toLatin1();  return;
            case QMetaType::QChar:      *((QChar*)Pointer)         = Value.isEmpty() ? 0 : Value.at(0);            return;
            case QMetaType::Bool:       *((bool*)Pointer)          = ToBool(Value);       return;
            case QMetaType::Short:      *((short*)Pointer)         = Value.toShort();     return;
            case QMetaType::UShort:     *((ushort*)Pointer)        = Value.toUShort();    return;
            case QMetaType::Int:        *((int*)Pointer)           = Value.toInt();       return;
            case QMetaType::UInt:       *((uint*)Pointer)          = Value.toUInt();      return;
            case QMetaType::Long:       *((long*)Pointer)          = Value.toLong();      return;
            case QMetaType::ULong:      *((ulong*)Pointer)         = Value.toULong();     return;
            case QMetaType::LongLong:   *((qlonglong*)Pointer)     = Value.toLongLong();  return;
            case QMetaType::ULongLong:  *((qulonglong*)Pointer)    = Value.toULongLong(); return;
            case QMetaType::Double:     *((double*)Pointer)        = Value.toDouble();    return;
            case QMetaType::Float:      *((float*)Pointer)         = Value.toFloat();     return;
            case QMetaType::QString:    *((QString*)Pointer)       = Value;               return;
            case QMetaType::QByteArray: *((QByteArray*)Pointer)    = Value.toUtf8();      return;
            case QMetaType::QTime:      *((QTime*)Pointer)         = QTime::fromString(Value, Qt::ISODate); return;
            case QMetaType::QDate:      *((QDate*)Pointer)         = QDate::fromString(Value, Qt::ISODate); return;
            case QMetaType::QDateTime:
            {
                QDateTime dt = QDateTime::fromString(Value, Qt::ISODate);
                dt.setTimeSpec(Qt::UTC);
                *((QDateTime*)Pointer) = dt;
                return;
            }
            default: break;
        }
    }

    static bool ToBool(const QString &Value)
    {
        if (Value.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare(QStringLiteral("y"), Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0)
            return true;
        return false;
    }

    /*! \brief Disable this method
    */
    void Disable(void)
    {
        m_allowedRequestTypes |= HTTPDisabled;
    }

    /*! \brief Enable this method
    */
    void Enable(void)
    {
        if (m_allowedRequestTypes & HTTPDisabled)
            m_allowedRequestTypes -= HTTPDisabled;
    }

    bool                m_valid;
    int                 m_index;
    QVector<QByteArray> m_names;
    QVector<int>        m_types;
    int                 m_allowedRequestTypes;
    QString             m_returnType;
    QMetaMethod         m_method;
};

/*! \class TorcHTTPService
 *
 * \todo Support for complex parameter types via RPC (e.g. array etc).
 * \todo ProcessRequest implicitly assumes JSON-RPC (though applies to much of the RPC code).
*/
TorcHTTPService::TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                                 const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcHTTPHandler(TORC_SERVICES_DIR + Signature, Name),
    m_httpServiceLock(QReadWriteLock::Recursive),
    m_parent(Parent),
    m_version(QStringLiteral("Unknown")),
    m_methods(),
    m_properties(),
    m_subscribers(),
    m_subscriberLock(QMutex::Recursive)
{
    static const QString defaultblacklisted(QStringLiteral("deleteLater,SubscriberDeleted,"));
    QStringList blacklist = (defaultblacklisted + Blacklist).split(',');

    m_parent->setObjectName(Name);

    // the parent MUST implement SubscriberDeleted.
    if (MetaObject.indexOfSlot(QMetaObject::normalizedSignature("SubscriberDeleted(QObject*)")) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Service '%1' has no SubscriberDeleted slot. This is a programmer error - exiting").arg(Name));
        QCoreApplication::exit(TORC_EXIT_UNKOWN_ERROR);
        return;
    }

    // determine version
    int index = MetaObject.indexOfClassInfo("Version");
    if (index > -1)
        m_version = MetaObject.classInfo(index).value();
    else
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Service '%1' is missing version information").arg(Name));

    // is this a secure service (all methods require authentication)
    bool secure = MetaObject.indexOfClassInfo("Secure") > -1;

    // build a list of metaobjects from all superclasses as well.
    QList<const QMetaObject*> metas;
    metas.append(&MetaObject);
    const QMetaObject* super = MetaObject.superClass();
    while (super)
    {
        metas.append(super);
        super = super->superClass();
    }

    // analyse available methods. Build the list from the top superclass 'down' to ensure we pick up
    // overriden slots and discard duplicates
    QListIterator<const QMetaObject*> it(metas);
    it.toBack();
    while (it.hasPrevious())
    {
        const QMetaObject *meta = it.previous();

        for (int i = 0; i < meta->methodCount(); ++i)
        {
            QMetaMethod method = meta->method(i);

            if ((method.methodType() == QMetaMethod::Slot) &&
                (method.access()     == QMetaMethod::Public))
            {
                QString name(method.methodSignature());
                name = name.section('(', 0, 0);

                // discard unwanted slots
                if (blacklist.contains(name))
                    continue;

                // any Q_CLASSINFO for this method?
                // current 'schema' allows specification of allowed HTTP methods (PUT, GET etc),
                // custom return types, which are used to improve the usability of maps and
                // lists when returned via XML, JSON, PLIST etc and requiring authentication (add AUTH to methods)
                QString returntype;
                int customallowed = HTTPUnknownType;

                // use the actual class metaObject - not the superclass
                int index = MetaObject.indexOfClassInfo(name.toLatin1());
                if (index > -1)
                {
                    QStringList infos = QString(MetaObject.classInfo(index).value()).split(',', QString::SkipEmptyParts);
                    foreach (const QString &info, infos)
                    {
                        if (info.startsWith(QStringLiteral("methods=")))
                            customallowed = TorcHTTPRequest::StringToAllowed(info.mid(8));
                        else if (info.startsWith(QStringLiteral("type=")))
                            returntype = info.mid(5);
                    }
                }

                // determine allowed request types
                int allowed = HTTPOptions;
                if (secure)
                    allowed |= HTTPAuth;
                if (customallowed != HTTPUnknownType)
                {
                    allowed |= customallowed;
                }
                else if (name.startsWith(QStringLiteral("Get"), Qt::CaseInsensitive))
                {
                    allowed |= HTTPGet | HTTPHead;
                }
                else if (name.startsWith(QStringLiteral("Set"), Qt::CaseInsensitive))
                {
                    // TODO Put or Post?? How to handle head requests for setters...
                    allowed |= HTTPPut;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unable to determine request types of method '%1' for '%2' - ignoring").arg(name, m_name));
                    continue;
                }

                if (allowed & HTTPAuth)
                    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("%1 requires authentication").arg(Signature + name));

                MethodParameters *parameters = new MethodParameters(i, method, allowed, returntype);

                if (parameters->m_valid)
                {
                    // check whether method has already been identified from superclass - need to match whole 'signature'
                    // not just name
                    if (m_methods.contains(name))
                    {
                        MethodParameters *existing = m_methods.value(name);
                        if (existing->m_method.methodSignature() == method.methodSignature() &&
                            existing->m_method.returnType()      == method.returnType())
                        {
                            LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Method '%1' in class '%2' already found in superclass - overriding")
                                .arg(method.methodSignature().constData(), meta->className()));
                            existing = m_methods.take(name);
                            delete existing;
                        }
                    }

                    m_methods.insert(name, parameters);
                }
                else
                {
                    delete parameters;
                }
            }
        }
    }

    // analyse properties from the full list of metaobjects
    int invalidindex = -1;
    foreach (const QMetaObject* meta, metas)
    {
        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i)
        {
            QMetaProperty property = meta->property(i);
            QString   propertyname(property.name());

            if (propertyname != QStringLiteral("objectName") && property.isReadable() && ((property.hasNotifySignal() && property.notifySignalIndex() > -1) || property.isConstant()))
            {
                // constant properties are given a signal index < 0
                if (property.notifySignalIndex() > -1)
                {
                    m_properties.insert(property.notifySignalIndex(), property.propertyIndex());
                    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Adding property '%1' with signal index %2").arg(property.name()).arg(property.notifySignalIndex()));
                }
                else
                {
                    m_properties.insert(invalidindex--, property.propertyIndex());
                    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Adding constant property '%1'").arg(property.name()));
                }
            }
        }
    }
}

TorcHTTPService::~TorcHTTPService()
{
    qDeleteAll(m_methods);
}

QString TorcHTTPService::GetUIName(void)
{
    return Name();
}

QString TorcHTTPService::GetPresentationURL(void)
{
    return QString();
}

void TorcHTTPService::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
{
    (void)PeerAddress;
    (void)PeerPort;
    (void)LocalAddress;
    (void)LocalPort;

    QString method = Request.GetMethod();
    HTTPRequestType type = Request.GetHTTPRequestType();

    if (method.compare(QStringLiteral("GetServiceVersion")) == 0)
    {
        if (type == HTTPOptions)
        {
            Request.SetStatus(HTTP_OK);
            Request.SetResponseType(HTTPResponseDefault);
            Request.SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
            return;
        }

        if (type != HTTPGet && type != HTTPHead)
        {
            Request.SetStatus(HTTP_BadRequest);
            Request.SetResponseType(HTTPResponseDefault);
            return;
        }

        Request.SetStatus(HTTP_OK);
        Request.Serialise(m_version, TORC_SERVICE_VERSION);
        return;
    }

    QMap<QString,MethodParameters*>::const_iterator it = m_methods.constFind(method);
    if (it != m_methods.constEnd())
    {
        // filter out invalid request types
        if ((!(type & (*it)->m_allowedRequestTypes)) ||
            (*it)->m_allowedRequestTypes & HTTPDisabled)
        {
            Request.SetStatus(HTTP_BadRequest);
            Request.SetResponseType(HTTPResponseDefault);
            return;
        }

        // handle OPTIONS
        if (type == HTTPOptions)
        {
            HandleOptions(Request, (*it)->m_allowedRequestTypes);
            return;
        }

        // reject requests that have a particular need for authorisation. We can only
        // check at this late stage but these should be in the minority
        if (!MethodIsAuthorised(Request, (*it)->m_allowedRequestTypes))
            return;

        QString type;
        bool    voidresult;
        QVariant result = (*it)->Invoke(m_parent, Request.Queries(), type, voidresult);

        // is there a result
        if (!voidresult)
        {
            // check for invocation errors
            if (result.type() == QVariant::Invalid)
            {
                Request.SetStatus(HTTP_BadRequest);
                Request.SetResponseType(HTTPResponseDefault);
                return;
            }

            Request.Serialise(result, type);
            Request.SetAllowGZip(true);
        }
        else
        {
            Request.SetResponseType(HTTPResponseNone);
        }

        Request.SetStatus(HTTP_OK);
    }
}

QVariantMap TorcHTTPService::ProcessRequest(const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated)
{
    QString method;
    int index = Method.lastIndexOf('/');
    if (index > -1)
        method = Method.mid(index + 1).trimmed();

    if (Connection && !method.isEmpty())
    {
        // find the correct method to invoke
        QMap<QString,MethodParameters*>::const_iterator it = m_methods.constFind(method);
        if (it != m_methods.constEnd())
        {
            // disallow methods based on state and authentication
            int types         = it.value()->m_allowedRequestTypes;
            bool disabled     = types & HTTPDisabled;
            bool unauthorised = !Authenticated && (types & HTTPAuth || types & HTTPPost || types & HTTPPut || types & HTTPDelete || types & HTTPUnknownType);

            if (disabled || unauthorised)
            {
                if (disabled)
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("'%1' method '%2' is disabled").arg(m_signature, method));
                else
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("'%1' method '%2' unauthorised").arg(m_signature, method));
                QVariantMap result;
                QVariantMap error;
                error.insert(QStringLiteral("code"), -401); // HTTP 401!
                error.insert(QStringLiteral("message"), QStringLiteral("Method not authorised"));
                result.insert(QStringLiteral("error"), error);
                return result;
            }
            else
            {
                // invoke it

                // convert the parameters
                QMap<QString,QString> params;
                if (Parameters.type() == QVariant::Map)
                {
                    QVariantMap map = Parameters.toMap();
                    QVariantMap::iterator it = map.begin();
                    for ( ; it != map.end(); ++it)
                        params.insert(it.key(), it.value().toString());
                }
                else if (Parameters.type() == QVariant::List)
                {
                    QVariantList list = Parameters.toList();
                    if (list.size() <= (*it)->m_names.size())
                    {
                        for (int i = 0; i < list.size(); ++i)
                            params.insert((*it)->m_names[i], list[i].toString());
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Too many parameters"));
                    }
                }
                else if (!Parameters.isNull())
                {
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unknown parameter variant"));
                }

                QString type;
                bool    voidresult = false;
                QVariant results = (*it)->Invoke(m_parent, params, type, voidresult);

                // check result
                if (!voidresult)
                {
                    // check for invocation errors
                    if (results.type() != QVariant::Invalid)
                    {
                        QVariantMap result;
                        result.insert(QStringLiteral("result"), results);
                        return result;
                    }

                    QVariantMap result;
                    QVariantMap error;
                    error.insert(QStringLiteral("code"), -32602);
                    error.insert(QStringLiteral("message"), QStringLiteral("Invalid params"));
                    result.insert(QStringLiteral("error"), error);
                    return result;
                }

                // JSON-RPC 2.0 specification makes no mention of void/null return types
                QVariantMap result;
                result.insert(QStringLiteral("result"), QStringLiteral("null"));
                return result;
            }
        }

        // implicit 'GetServiceVersion' method
        if (method.compare(QStringLiteral("GetServiceVersion")) == 0)
        {
            QVariantMap result;
            QVariantMap version;
            version.insert(TORC_SERVICE_VERSION, m_version);
            result.insert(QStringLiteral("result"), version);
            return result;
        }
        // implicit 'Subscribe' method
        else if (method.compare(QStringLiteral("Subscribe")) == 0)
        {
            // ensure the 'receiver' has all of the right slots
            int change = Connection->metaObject()->indexOfSlot(QMetaObject::normalizedSignature("PropertyChanged()"));
            if (change > -1)
            {
                // this method is not thread-safe and is called from multiple threads so lock the subscribers
                QMutexLocker locker(&m_subscriberLock);

                if (!m_subscribers.contains(Connection))
                {
                    LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("New subscription for '%1'").arg(m_signature));
                    m_subscribers.append(Connection);

                    // notify success and provide appropriate details about properties, notifications, get'ers etc
                    QMap<int,int>::const_iterator it = m_properties.constBegin();
                    for ( ; it != m_properties.constEnd(); ++it)
                    {
                        // and connect property change notifications to the one slot
                        // NB we use the parent's metaObject here - not the staticMetaObject (or m_metaObject)
                        if (it.key() > -1)
                            QObject::connect(m_parent, m_parent->metaObject()->method(it.key()), Connection, Connection->metaObject()->method(change));

                        // clean up subscriptions if the subscriber is deleted
                        QObject::connect(Connection, SIGNAL(destroyed(QObject*)), m_parent, SLOT(SubscriberDeleted(QObject*)));// clazy:exclude=old-style-connect
                    }

                    QVariantMap result;
                    result.insert(QStringLiteral("result"), GetServiceDetails());
                    return result;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Connection is already subscribed to '%1'").arg(m_signature));
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Subscription request for connection without correct methods"));
            }

            QVariantMap result;
            QVariantMap error;
            error.insert(QStringLiteral("code"), -32603);
            error.insert(QStringLiteral("message"), "Internal error");
            result.insert(QStringLiteral("error"), error);
            return result;
        }
        // implicit 'Unsubscribe' method
        else if (method.compare(QStringLiteral("Unsubscribe")) == 0)
        {
            QMutexLocker locker(&m_subscriberLock);

            if (m_subscribers.contains(Connection))
            {
                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Removed subscription for '%1'").arg(m_signature));

                // disconnect all change signals
                m_parent->disconnect(Connection);

                // remove the subscriber
                m_subscribers.removeAll(Connection);

                // return success
                QVariantMap result;
                result.insert(QStringLiteral("result"), 1);
                return result;
            }

            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Connection is not subscribed to '%1'").arg(m_signature));

            QVariantMap result;
            QVariantMap error;
            error.insert(QStringLiteral("code"), -32603);
            error.insert(QStringLiteral("message"), "Internal error");
            result.insert(QStringLiteral("error"), error);
            return result;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("'%1' service has no '%2' method").arg(m_signature, method));

    QVariantMap result;
    QVariantMap error;
    error.insert(QStringLiteral("code"), -32601);
    error.insert(QStringLiteral("message"), QStringLiteral("Method not found"));
    result.insert(QStringLiteral("error"), error);
    return result;
}

/*! \brief Return a QVariantMap describing the services methods and properties.
 *
 * This is sent to new subscribers immediately after a successful subscription
 * to provide them with the full 'API' and current state.
 *
 * It is also used by the TorcHTMLServicesHelp object to retrieve a service description
 * (as used by the API helper screen on the web frontend).
 */
QVariantMap TorcHTTPService::GetServiceDetails(void)
{
    // no need for locking here

    QVariantMap details;
    QVariantMap properties;
    QVariantMap methods;

    QMap<int,int>::const_iterator it = m_properties.constBegin();
    for ( ; it != m_properties.constEnd(); ++it)
    {
        // NB for some reason, QMetaProperty doesn't provide the QMetaMethod for the read and write
        // slots, so try to infer them (and check the result)
        QMetaProperty property = m_parent->metaObject()->property(it.value());
        QVariantMap description;
        QString name = QString::fromLatin1(property.name());
        if (name.isEmpty())
            continue;

        description.insert(QStringLiteral("notification"), QString::fromLatin1(m_parent->metaObject()->method(it.key()).name()));

        // a property is always readable
        QString camelname = name.at(0).toUpper() + name.mid(1);
        QString read = QString(QStringLiteral("Get")) + camelname;

        if (m_parent->metaObject()->indexOfSlot(QMetaObject::normalizedSignature(QString(read + "()").toLatin1())) > -1)
            description.insert(QStringLiteral("read"), read);
        else
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to deduce 'read' slot for property '%1' in service '%2'").arg(name, m_signature));

        // for writable properties, we need to infer the signature including the type
        if (property.isWritable())
        {
            QString write = QString(QStringLiteral("Set")) + camelname;
            if (m_parent->metaObject()->indexOfSlot(QMetaObject::normalizedSignature(QStringLiteral("%1(%2)").arg(write, property.typeName()).toLatin1())) > -1)
                description.insert(QStringLiteral("write"), write);
            else
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to deduce 'write' slot for property '%1' in service '%2'").arg(name, m_signature));
        }

        // and add the initial value
        description.insert(QStringLiteral("value"), property.read(m_parent));

        properties.insert(name, description);
    }

    QVariantList params;
    QMap<QString,MethodParameters*>::const_iterator it2 = m_methods.constBegin();
    for ( ; it2 != m_methods.constEnd(); ++it2)
    {
        QVariantMap map;

        MethodParameters *parameters = it2.value();
        for (int i = 1; i < parameters->m_types.size(); ++i)
            params.append(parameters->m_names.value(i).constData());
        map.insert(QStringLiteral("params"), params);
        map.insert(QStringLiteral("returns"), TorcJSONRPC::QMetaTypetoJavascriptType(parameters->m_types[0]));
        methods.insert(it2.key(), map);
        params.clear();
    }

    // and implicit Subscribe/Unsubscribe/GetServiceVersion
    // NB these aren't implemented as public slots and property (for serviceVersion)
    // as TorcHTTPService is not a QObject. Not ideal as the full API is not
    // visible in the code.
    params.clear();
    QVariant returns("object");

    QVariantMap subscribe;
    subscribe.insert(QStringLiteral("params"), params);
    subscribe.insert(QStringLiteral("returns"), returns);
    methods.insert(QStringLiteral("Subscribe"), subscribe);

    QVariantMap unsubscribe;
    unsubscribe.insert(QStringLiteral("params"), params);
    unsubscribe.insert(QStringLiteral("returns"), returns);
    methods.insert(QStringLiteral("Unsubscribe"), unsubscribe);

    QVariantMap serviceversion;
    serviceversion.insert(QStringLiteral("params"), params);
    serviceversion.insert(QStringLiteral("returns"), TorcJSONRPC::QMetaTypetoJavascriptType(QMetaType::QString));
    methods.insert(QStringLiteral("GetServiceVersion"), serviceversion);

    // and the implicit version property
    QVariantMap description;
    description.insert(QStringLiteral("read"), QStringLiteral("GetServiceVersion"));
    description.insert(QStringLiteral("value"), m_version);
    properties.insert(QStringLiteral("serviceVersion"), description);

    details.insert(QStringLiteral("properties"), properties);
    details.insert(QStringLiteral("methods"), methods);
    return details;
}

QString TorcHTTPService::GetMethod(int Index)
{
    if (Index > -1 && Index < m_parent->metaObject()->methodCount())
        return QString::fromLatin1(m_parent->metaObject()->method(Index).name());

    return QString();
}

/*! \brief Get the value of a given property.
 *
 * \note Index is the method index for the property's getter - not the property itself.
*/
QVariant TorcHTTPService::GetProperty(int Index)
{
    QVariant result;

    if (Index > -1 && m_properties.contains(Index))
        result = m_parent->metaObject()->property(m_properties.value(Index)).read(m_parent);
    else
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to retrieve property"));

    return result;
}

void TorcHTTPService::HandleSubscriberDeleted(QObject *Subscriber)
{
    QMutexLocker locker(&m_subscriberLock);

    if (Subscriber && m_subscribers.contains(Subscriber))
    {
        LOG(VB_GENERAL, LOG_DEBUG, QStringLiteral("Subscriber deleted - cancelling subscription"));
        m_subscribers.removeAll(Subscriber);
    }
}
