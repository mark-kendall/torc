/* Class TorcWebSocketThread
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

// Qt
#include <QFile>
#include <QMutex>
#include <QSslKey>
#include <QSslCipher>
#include <QSslConfiguration>

// Torc
#include "torclogging.h"
#include "torclocaldefs.h"
#include "torcdirectories.h"
#include "torcwebsocketthread.h"

// SSL
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/bn.h>

// STL - for chmod
#include <sys/stat.h>

/*! \class TorcWebSocketThread
 *  \brief Wraps a TorcQThread around a TorcWebsocket
 *
 * A simple wrapper that creates and runs a TorcWebSocket in its own thread and passes through RemoteRequest
 * and CancelRequest calls. TorcWebSocket is parent 'agnostic'.
 *
 * The two different constructors either upgrade an incoming socket to a WebSocket or create a socket and
 * connect to a remote server.
 *
 * \sa TorcWebsocket
 * \sa TorcQThread
 * \sa TorcHTTPRequest
 *
 * \todo Investigate edge case leak when RemoteRequest or CancelRequest possibly might not be delivered during socket
 *       or application shutdown. Is this a real issue? (N.B. Only applies when signals are asynchronous
 *       i.e. running inside TorcWebSocketThread).
*/

TorcWebSocketThread::TorcWebSocketThread(qintptr SocketDescriptor, bool Secure)
  : TorcQThread(QStringLiteral("SocketIn")),
    m_webSocket(nullptr),
    m_secure(Secure),
    m_socketDescriptor(SocketDescriptor),
    m_address(QHostAddress::Null),
    m_port(0),
    m_protocol(TorcWebSocketReader::SubProtocolNone)
{
}

TorcWebSocketThread::TorcWebSocketThread(const QHostAddress &Address, quint16 Port, bool Secure, TorcWebSocketReader::WSSubProtocol Protocol)
  : TorcQThread(QStringLiteral("SocketOut")),
    m_webSocket(nullptr),
    m_secure(Secure),
    m_socketDescriptor(0),
    m_address(Address),
    m_port(Port),
    m_protocol(Protocol)
{
}

int SSLCallback(int, int, BN_GENCB*)
{
    static int count = 0;
    if (!(++count % 20))
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Key generation..."));
    return 1;
}

bool TorcWebSocketThread::CreateCerts(const QString &CertFile, const QString &KeyFile)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Generating RSA key"));

    RSA *rsa = RSA_new();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    BN_GENCB cb;
    BN_GENCB_set(&cb, &SSLCallback, nullptr);
#else
    BN_GENCB *cb = BN_GENCB_new();
    BN_GENCB_set(cb, &SSLCallback, nullptr);
#endif
    BIGNUM *e;
    e = BN_new();
    BN_set_word(e, RSA_F4);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    RSA_generate_key_ex(rsa, 4096, e, &cb);
#else
    RSA_generate_key_ex(rsa, 4096, e, cb);
#endif
    BN_free(e);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    BN_GENCB_free(cb);
#endif
    if (nullptr == rsa)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to generate RSA key"));
        return false;
    }

    EVP_PKEY *privatekey = EVP_PKEY_new();
    if(!EVP_PKEY_assign_RSA(privatekey, rsa))
    {
        EVP_PKEY_free(privatekey);
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create RSA key"));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Generating X509 certificate"));
    X509 *x509 = X509_new();
    // we need a unique serial number (or more strictly a unique combination of CA and serial number)
    QString timenow = QString::number(QDateTime::currentMSecsSinceEpoch());
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("New cert serial number: %1").arg(timenow));
    BIGNUM *bn = BN_new();
    if (BN_dec2bn(&bn, timenow.toLatin1().constData()) != timenow.size())
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Conversion error"));
    ASN1_INTEGER *sno = ASN1_INTEGER_new();
    sno = BN_to_ASN1_INTEGER(bn, sno);
    X509_set_serialNumber(x509, sno);
    BN_free(bn);
    ASN1_INTEGER_free(sno);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 315360000L); // valid for 10 years!
    X509_set_pubkey(x509, privatekey);
    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (unsigned char *)"GB",           -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (unsigned char *)"SelfSignedCo", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (unsigned char *)"SelfSignedCo", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"localhost",    -1, -1, 0);
    X509_set_issuer_name(x509, name);
    if(!X509_sign(x509, privatekey, EVP_sha256()))
    {
        X509_free(x509);
        EVP_PKEY_free(privatekey);
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to sign certificate"));
        return false;
    }

    FILE* certfile = fopen(CertFile.toLocal8Bit().constData(), "wb");
    if (!certfile)
    {
        X509_free(x509);
        EVP_PKEY_free(privatekey);
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to open '%1' for writing").arg(CertFile));
        return false;
    }
    bool success = PEM_write_X509(certfile, x509);
    fclose(certfile);
    if (!success)
    {
        X509_free(x509);
        EVP_PKEY_free(privatekey);
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to write to '%1'").arg(CertFile));
        return false;
    }

    FILE* keyfile = fopen(KeyFile.toLocal8Bit().constData(), "wb");
    if (!keyfile)
    {
        X509_free(x509);
        EVP_PKEY_free(privatekey);
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to open '%1' for writing").arg(KeyFile));
        return false;
    }

    success = PEM_write_PrivateKey(keyfile, privatekey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(keyfile);
    X509_free(x509);
    EVP_PKEY_free(privatekey);

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to write to '%1'").arg(KeyFile));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Cert file saved as '%1'").arg(CertFile));
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Key file saved as '%1'").arg(KeyFile));

    if (chmod(CertFile.toLocal8Bit().constData(), S_IRUSR | S_IWUSR) != 0)
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to set permissions for '%1' - this is not fatal but may present a security risk").arg(CertFile));
    if (chmod(KeyFile.toLocal8Bit().constData(),  S_IRUSR | S_IWUSR) != 0)
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to set permissions for '%1' - this is not fatal but may present a security risk").arg(KeyFile));
    return true;
}

void TorcWebSocketThread::SetupSSL(void)
{
    static bool   SSLDefaultsSet = false;
    static QMutex SSLDefaultsLock(QMutex::Recursive);

    // block any incoming sockets until cert creation is complete - it can take a few seconds
    QMutexLocker locker(&SSLDefaultsLock);
    if (SSLDefaultsSet)
        return;

    SSLDefaultsSet = true;
    QSslConfiguration config;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    config.setProtocol(QSsl::TlsV1_2OrLater);
    config.setCiphers(QSslConfiguration::supportedCiphers());
#else
    config.setProtocol(QSsl::TlsV1_2);
    config.setCiphers(QSslSocket::supportedCiphers());
#endif

    QString certlocation = GetTorcConfigDir() + "/" + TORC_TORC + ".cert";
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSL: looking for cert in '%1'").arg(certlocation));
    QString keylocation  = GetTorcConfigDir() + "/" + TORC_TORC + ".key";
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSL: looking for key in '%1'").arg(keylocation));

    bool create = false;
    if (!QFile::exists(certlocation))
    {
        create = true;
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to find cert"));
    }
    if (!QFile::exists(keylocation))
    {
        create = true;
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to find key"));
    }

    if (create && !CreateCerts(certlocation, keylocation))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("SSL key/cert creation failed - server connections will fail"));
        return;
    }

    QFile certFile(certlocation);
    certFile.open(QIODevice::ReadOnly);
    if (certFile.isOpen())
    {
        QSslCertificate certificate(&certFile, QSsl::Pem);
        if (!certificate.isNull())
        {
            config.setLocalCertificate(certificate);
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSL: cert loaded"));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("SSL: error loading/reading cert file"));
        }
        certFile.close();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("SSL: failed to open cert file for reading"));
    }

    QFile keyFile(keylocation);
    keyFile.open(QIODevice::ReadOnly);
    if (keyFile.isOpen())
    {
        QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
        if (!key.isNull())
        {
            config.setPrivateKey(key);
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSL: key loaded"));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("SSL: error loading/reading key file"));
        }
        keyFile.close();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("SSL: failed to open key file for reading"));
    }
    QSslConfiguration::setDefaultConfiguration(config);
}

void TorcWebSocketThread::Start(void)
{
    // one off SSL default configuration when needed
    if (m_secure && m_socketDescriptor)
        SetupSSL();

    if (m_socketDescriptor)
        m_webSocket = new TorcWebSocket(this, m_socketDescriptor, m_secure);
    else
        m_webSocket = new TorcWebSocket(this, m_address, m_port, m_secure, m_protocol);

    connect(m_webSocket, &TorcWebSocket::ConnectionEstablished, this, &TorcWebSocketThread::ConnectionEstablished);
    connect(m_webSocket, &TorcWebSocket::ConnectionUpgraded,    this, &TorcWebSocketThread::ConnectionUpgraded);
    connect(m_webSocket, &TorcWebSocket::Disconnected,          this, &TorcWebSocketThread::quit);
    // the websocket is created in its own thread so these signals will be delivered into the correct thread.
    connect(this, &TorcWebSocketThread::RemoteRequestSignal, m_webSocket, &TorcWebSocket::RemoteRequest);
    connect(this, &TorcWebSocketThread::CancelRequestSignal, m_webSocket, &TorcWebSocket::CancelRequest);
    m_webSocket->Start();
}

void TorcWebSocketThread::Finish(void)
{
    if (m_webSocket)
        delete m_webSocket;
    m_webSocket = nullptr;
}

bool TorcWebSocketThread::IsSecure(void)
{
    return m_secure;
}

void TorcWebSocketThread::RemoteRequest(TorcRPCRequest *Request)
{
    emit RemoteRequestSignal(Request);
}

void TorcWebSocketThread::CancelRequest(TorcRPCRequest *Request)
{
    emit CancelRequestSignal(Request);
}
