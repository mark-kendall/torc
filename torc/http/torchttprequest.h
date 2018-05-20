#ifndef TORCHTTPREQUEST_H
#define TORCHTTPREQUEST_H

// Qt
#include <QMap>
#include <QPair>
#include <QString>
#include <QDateTime>

// Torc
#include "torchttpreader.h"

class TorcSerialiser;
class QTcpSocket;
class QFile;

typedef enum
{
    HTTPRequest,
    HTTPResponse
} HTTPType;

typedef enum
{
    HTTPUnknownType = (0 << 0),
    HTTPHead        = (1 << 0),
    HTTPGet         = (1 << 1),
    HTTPPost        = (1 << 2),
    HTTPPut         = (1 << 3),
    HTTPDelete      = (1 << 4),
    HTTPOptions     = (1 << 5),
    HTTPDisabled    = (1 << 6),
    HTTPAuth        = (1 << 7)  // require authentication
} HTTPRequestType;

typedef enum
{
    HTTPResponseUnknown = 0,
    HTTPResponseNone,
    HTTPResponseDefault,
    HTTPResponseHTML,
    HTTPResponseXML,
    HTTPResponseJSON,
    HTTPResponseJSONJavascript,
    HTTPResponsePList,
    HTTPResponseBinaryPList,
    HTTPResponsePListApple,
    HTTPResponseBinaryPListApple,
    HTTPResponsePlainText
} HTTPResponseType;

typedef enum
{
    HTTPUnknownProtocol = 0,
    HTTPZeroDotNine,
    HTTPOneDotZero,
    HTTPOneDotOne
} HTTPProtocol;

typedef enum
{
    HTTP_SwitchingProtocols  = 101,
    HTTP_OK                  = 200,
    HTTP_PartialContent      = 206,
    HTTP_MovedPermanently    = 301,
    HTTP_NotModified         = 304,
    HTTP_BadRequest          = 400,
    HTTP_Unauthorized        = 401,
    HTTP_Forbidden           = 402,
    HTTP_NotFound            = 404,
    HTTP_MethodNotAllowed    = 405,
    HTTP_RequestedRangeNotSatisfiable = 416,
    HTTP_TooManyRequests     = 429,
    HTTP_InternalServerError = 500,
    HTTP_NotImplemented      = 501,
    HTTP_BadGateway          = 502,
    HTTP_ServiceUnavailable  = 503,
    HTTP_NetworkAuthenticationRequired = 511
} HTTPStatus;

typedef enum
{
    HTTPConnectionClose     = 0,
    HTTPConnectionKeepAlive = 1,
    HTTPConnectionUpgrade   = 2
} HTTPConnection;

typedef enum
{
    HTTPCacheNone           = (1 << 0),
    HTTPCacheShortLife      = (1 << 1),
    HTTPCacheLongLife       = (1 << 2),
    HTTPCacheLastModified   = (1 << 3),
    HTTPCacheETag           = (1 << 4)
} HTTPCacheing;

typedef enum
{
    HTTPNotAuthorised       = 0,
    HTTPPreAuthorised       = 1,
    HTTPAuthorised          = 2,
    HTTPAuthorisedStale     = 3
} HTTPAuthorisation;

#define READ_CHUNK_SIZE (1024 *64)

class TorcHTTPRequest
{
    friend class TorcWebSocket;

  public:
    static HTTPRequestType RequestTypeFromString    (const QString &Type);
    static QString         RequestTypeToString      (HTTPRequestType Type);
    static HTTPProtocol    ProtocolFromString       (const QString &Protocol);
    static HTTPStatus      StatusFromString         (const QString &Status);
    static QString         ProtocolToString         (HTTPProtocol Protocol);
    static QString         StatusToString           (HTTPStatus   Status);
    static QString         ResponseTypeToString     (HTTPResponseType Response);
    static QString         AllowedToString          (int Allowed);
    static QString         ConnectionToString       (HTTPConnection Connection);
    static int             StringToAllowed          (const QString &Allowed);
    static QList<QPair<quint64,quint64> > StringToRanges (const QString &Ranges, qint64 Size, qint64& SizeToSend);
    static QString         RangeToString            (const QPair<quint64,quint64> &Range, qint64 Size);

    static char            DateFormat[];

  public:
    explicit TorcHTTPRequest(TorcHTTPReader *Reader);

    void                   SetConnection            (HTTPConnection Connection);
    void                   SetStatus                (HTTPStatus Status);
    void                   SetResponseType          (HTTPResponseType Type);
    void                   SetResponseContent       (QByteArray *Content);
    void                   SetResponseFile          (QFile *File);
    void                   SetResponseHeader        (const QString &Header, const QString &Value);
    void                   SetAllowed               (int Allowed);
    void                   SetAllowGZip             (bool Allowed);
    void                   SetCache                 (int Cache, const QString Tag = QString(""));
    void                   SetSecure                (bool Secure);
    bool                   GetSecure                (void);
    HTTPStatus             GetHTTPStatus            (void) const;
    HTTPType               GetHTTPType              (void) const;
    HTTPRequestType        GetHTTPRequestType       (void) const;
    HTTPProtocol           GetHTTPProtocol          (void) const;
    QString                GetUrl                   (void) const;
    QString                GetPath                  (void) const;
    QString                GetMethod                (void) const;
    QString                GetCache                 (void) const;
    const QMap<QString,QString>* Headers            (void) const;
    const QMap<QString,QString>& Queries            (void) const;
    void                   Respond                  (QTcpSocket *Socket);
    void                   Redirected               (const QString &Redirected);
    TorcSerialiser*        GetSerialiser            (void);
    bool                   Unmodified               (const QDateTime &LastModified);
    bool                   Unmodified               (void);
    void                   Authorise                (HTTPAuthorisation Authorisation);
    HTTPAuthorisation      IsAuthorised             (void) const;

  protected:
   ~TorcHTTPRequest();
    void                   Initialise               (const QString &Method);

  protected:
    QString                m_fullUrl;
    QString                m_path;
    QString                m_method;
    QString                m_query;
    QString                m_redirectedTo;
    HTTPType               m_type;
    HTTPRequestType        m_requestType;
    HTTPProtocol           m_protocol;
    HTTPConnection         m_connection;
    QList<QPair<quint64,quint64> > m_ranges;
    QMap<QString,QString> *m_headers;
    QMap<QString,QString>  m_queries;
    QByteArray            *m_content;
    bool                   m_secure;

    bool                   m_allowGZip;
    int                    m_allowed;
    HTTPAuthorisation      m_authorised;
    HTTPResponseType       m_responseType;
    int                    m_cache;
    QString                m_cacheTag;
    HTTPStatus             m_responseStatus;
    QByteArray            *m_responseContent;
    QFile                 *m_responseFile;
    QMap<QString,QString> *m_responseHeaders;

  private:
    Q_DISABLE_COPY(TorcHTTPRequest)
};

#endif // TORCHTTPREQUEST_H
