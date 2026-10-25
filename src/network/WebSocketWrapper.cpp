#include "WebSocketWrapper.h"
#include "Logger.h"
#include <QNetworkRequest>

WebSocketWrapper::WebSocketWrapper(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_reconnectTimer(new QTimer(this))
{
    connect(m_socket, &QWebSocket::connected, this, &WebSocketWrapper::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &WebSocketWrapper::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &WebSocketWrapper::onTextMessageReceived);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &WebSocketWrapper::onBinaryMessageReceived);
    connect(m_socket, &QWebSocket::errorOccurred, this, &WebSocketWrapper::onError);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &WebSocketWrapper::onReconnectTimeout);
}

WebSocketWrapper::~WebSocketWrapper()
{
    m_manualClose = true;
    m_reconnectTimer->stop();
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->close();
    }
}

void WebSocketWrapper::connectUrl(const QUrl& url, const QMap<QString, QString>& headers)
{
    m_targetUrl = url;
    m_headers = headers;
    m_manualClose = false;
    m_reconnectIntervalMs = m_initialIntervalMs;

    emit statusChanged(PlatformStatus::CONNECTING);

    QNetworkRequest request(m_targetUrl);
    if (m_headers.contains("User-Agent"))
    {
        request.setHeader(QNetworkRequest::UserAgentHeader, m_headers["User-Agent"]);
    }
    else
    {
        request.setHeader(QNetworkRequest::UserAgentHeader, 
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36");
    }

    for (auto it = m_headers.begin(); it != m_headers.end(); ++it)
    {
        if (it.key().compare("User-Agent", Qt::CaseInsensitive) != 0)
        {
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }
    }

    Logger::instance().info(m_name, QString("Connecting to WebSocket: %1").arg(m_targetUrl.toString()));
    m_socket->open(request);
}

void WebSocketWrapper::disconnectSocket()
{
    m_manualClose = true;
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_socket->close();
    }
}

void WebSocketWrapper::sendTextMessage(const QString& message)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->sendTextMessage(message);
    }
}

void WebSocketWrapper::sendBinaryMessage(const QByteArray& data)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->sendBinaryMessage(data);
    }
}

bool WebSocketWrapper::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void WebSocketWrapper::setAutoReconnect(bool enable, int initialIntervalMs, int maxIntervalMs)
{
    m_autoReconnect = enable;
    m_initialIntervalMs = initialIntervalMs;
    m_maxIntervalMs = maxIntervalMs;
}

void WebSocketWrapper::onConnected()
{
    Logger::instance().info(m_name, "WebSocket connected successfully");
    m_reconnectIntervalMs = m_initialIntervalMs;
    m_reconnectTimer->stop();
    emit connected();
    emit statusChanged(PlatformStatus::ONLINE);
}

void WebSocketWrapper::onDisconnected()
{
    Logger::instance().warn(m_name, QString("WebSocket disconnected. ManualClose=%1, AutoReconnect=%2")
        .arg(m_manualClose ? "true" : "false", m_autoReconnect ? "true" : "false"));

    emit disconnected();

    if (!m_manualClose && m_autoReconnect && m_targetUrl.isValid())
    {
        emit statusChanged(PlatformStatus::RECONNECTING);
        Logger::instance().info(m_name, QString("Scheduling reconnect in %1 ms...").arg(m_reconnectIntervalMs));
        m_reconnectTimer->start(m_reconnectIntervalMs);
        m_reconnectIntervalMs = qMin(m_reconnectIntervalMs * 2, m_maxIntervalMs);
    }
    else
    {
        emit statusChanged(PlatformStatus::OFFLINE);
    }
}

void WebSocketWrapper::onTextMessageReceived(const QString& message)
{
    emit textMessageReceived(message);
}

void WebSocketWrapper::onBinaryMessageReceived(const QByteArray& message)
{
    emit binaryMessageReceived(message);
}

void WebSocketWrapper::onError(QAbstractSocket::SocketError error)
{
    QString errStr = m_socket->errorString();
    Logger::instance().error(m_name, QString("WebSocket error (%1): %2").arg(static_cast<int>(error)).arg(errStr));
    emit errorOccurred(errStr);
}

void WebSocketWrapper::onReconnectTimeout()
{
    if (!m_manualClose && m_autoReconnect && m_targetUrl.isValid())
    {
        Logger::instance().info(m_name, "Attempting reconnect...");
        connectUrl(m_targetUrl, m_headers);
    }
}
