#include "LocalWebServer.h"
#include "ChatManager.h"
#include "EventManager.h"
#include "Logger.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>

LocalWebServer& LocalWebServer::instance()
{
    static LocalWebServer s_instance;
    return s_instance;
}

LocalWebServer::LocalWebServer(QObject* parent)
    : QObject(parent)
    , m_httpServer(new QTcpServer(this))
    , m_wsServer(new QWebSocketServer("MultiChatWebSocketServer", QWebSocketServer::NonSecureMode, this))
{
    connect(m_httpServer, &QTcpServer::newConnection, this, &LocalWebServer::onNewHttpConnection);
    connect(m_wsServer, &QWebSocketServer::newConnection, this, &LocalWebServer::onNewWebSocketConnection);

    connect(&ChatManager::instance(), &ChatManager::messageReceived, this, &LocalWebServer::onChatMessageReceived);
    connect(&EventManager::instance(), &EventManager::viewerCountUpdated, this, &LocalWebServer::onViewerCountUpdated);
    connect(&EventManager::instance(), &EventManager::platformStatusChanged, this, &LocalWebServer::onPlatformStatusChanged);
}

LocalWebServer::~LocalWebServer()
{
    stopServer();
}

bool LocalWebServer::startServer(quint16 httpPort, quint16 wsPort)
{
    if (m_isRunning)
    {
        stopServer();
    }

    m_httpPort = httpPort;
    m_wsPort = wsPort;

    if (!m_httpServer->listen(QHostAddress::AnyIPv4, m_httpPort))
    {
        QString err = QString("No se pudo iniciar el servidor HTTP en el puerto %1: %2")
            .arg(m_httpPort).arg(m_httpServer->errorString());
        Logger::instance().error("LocalWebServer", err);
        emit errorOccurred(err);
        return false;
    }

    if (!m_wsServer->listen(QHostAddress::AnyIPv4, m_wsPort))
    {
        QString err = QString("No se pudo iniciar el servidor WebSocket en el puerto %1: %2")
            .arg(m_wsPort).arg(m_wsServer->errorString());
        Logger::instance().error("LocalWebServer", err);
        m_httpServer->close();
        emit errorOccurred(err);
        return false;
    }

    m_isRunning = true;
    Logger::instance().info("LocalWebServer", QString("Servidor Local Web iniciado: HTTP en puerto %1, WebSocket en puerto %2")
        .arg(m_httpPort).arg(m_wsPort));
    Logger::instance().info("LocalWebServer", QString("Acceso Local: %1 | Acceso Red: %2")
        .arg(getLocalUrl(), getNetworkUrl()));

    emit serverStarted(getLocalUrl(), getNetworkUrl());
    emit clientCountChanged(0);
    return true;
}

void LocalWebServer::stopServer()
{
    if (!m_isRunning) return;

    for (QWebSocket* client : m_wsClients)
    {
        if (client)
        {
            client->close();
            client->deleteLater();
        }
    }
    m_wsClients.clear();

    if (m_httpServer->isListening())
    {
        m_httpServer->close();
    }
    if (m_wsServer->isListening())
    {
        m_wsServer->close();
    }

    m_isRunning = false;
    Logger::instance().info("LocalWebServer", "Servidor Local Web detenido.");
    emit serverStopped();
    emit clientCountChanged(0);
}

QString LocalWebServer::getLocalIpAddress() const
{
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& addr : addresses)
    {
        if (!addr.isLoopback() && addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            QString ip = addr.toString();
            if (ip.startsWith("192.168.") || ip.startsWith("10.") || ip.startsWith("172."))
            {
                return ip;
            }
        }
    }
    for (const QHostAddress& addr : addresses)
    {
        if (!addr.isLoopback() && addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            return addr.toString();
        }
    }
    return "127.0.0.1";
}

QString LocalWebServer::getLocalUrl() const
{
    return QString("http://localhost:%1").arg(m_httpPort);
}

QString LocalWebServer::getNetworkUrl() const
{
    return QString("http://%1:%2").arg(getLocalIpAddress()).arg(m_httpPort);
}

QString LocalWebServer::getOverlayUrl() const
{
    return QString("http://localhost:%1/?overlay=true").arg(m_httpPort);
}

int LocalWebServer::clientCount() const
{
    return m_wsClients.size();
}

void LocalWebServer::onNewHttpConnection()
{
    while (m_httpServer->hasPendingConnections())
    {
        QTcpSocket* socket = m_httpServer->nextPendingConnection();
        if (!socket) continue;

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handleHttpRequest(socket);
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void LocalWebServer::handleHttpRequest(QTcpSocket* socket)
{
    if (!socket || !socket->isOpen()) return;

    QByteArray requestData = socket->readAll();
    QString request = QString::fromUtf8(requestData);
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) return;

    QString firstLine = lines.first();
    QStringList parts = firstLine.split(" ");
    QString path = parts.size() > 1 ? parts[1] : "/";

    if (path.startsWith("/api/status"))
    {
        QJsonObject root;
        root["running"] = m_isRunning;
        root["httpPort"] = m_httpPort;
        root["wsPort"] = m_wsPort;
        root["clients"] = m_wsClients.size();
        root["localIp"] = getLocalIpAddress();
        root["totalViewers"] = EventManager::instance().getTotalViewerCount();

        QByteArray body = QJsonDocument(root).toJson();
        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json; charset=utf-8\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Connection: close\r\n"
                              "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    QByteArray body = getIndexHtml();
    QByteArray response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Connection: close\r\n"
                          "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void LocalWebServer::onNewWebSocketConnection()
{
    while (m_wsServer->hasPendingConnections())
    {
        QWebSocket* socket = m_wsServer->nextPendingConnection();
        if (!socket) continue;

        m_wsClients.append(socket);
        connect(socket, &QWebSocket::disconnected, this, &LocalWebServer::onWebSocketDisconnected);

        Logger::instance().info("LocalWebServer", QString("Nuevo cliente WebSocket conectado desde: %1:%2 (Total: %3)")
            .arg(socket->peerAddress().toString()).arg(socket->peerPort()).arg(m_wsClients.size()));
        emit clientCountChanged(m_wsClients.size());

        // Send initial state with recent messages and current viewer/status counts
        QJsonObject initObj;
        initObj["type"] = "init";
        initObj["totalViewers"] = EventManager::instance().getTotalViewerCount();

        QJsonObject statuses;
        statuses["Twitch"] = platformStatusToString(EventManager::instance().getPlatformStatus("Twitch"));
        statuses["YouTube"] = platformStatusToString(EventManager::instance().getPlatformStatus("YouTube"));
        statuses["Kick"] = platformStatusToString(EventManager::instance().getPlatformStatus("Kick"));
        statuses["TikTok"] = platformStatusToString(EventManager::instance().getPlatformStatus("TikTok"));
        initObj["statuses"] = statuses;

        QJsonObject viewers;
        viewers["Twitch"] = EventManager::instance().getViewerCount("Twitch");
        viewers["YouTube"] = EventManager::instance().getViewerCount("YouTube");
        viewers["Kick"] = EventManager::instance().getViewerCount("Kick");
        viewers["TikTok"] = EventManager::instance().getViewerCount("TikTok");
        initObj["viewers"] = viewers;

        QJsonArray recentMsgs;
        const auto all = ChatManager::instance().getAllMessages();
        int startIndex = qMax(0, all.size() - 40);
        for (int i = startIndex; i < all.size(); ++i)
        {
            recentMsgs.append(messageToJson(all[i]));
        }
        initObj["recentMessages"] = recentMsgs;

        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(initObj).toJson(QJsonDocument::Compact)));
    }
}

void LocalWebServer::onWebSocketDisconnected()
{
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (socket)
    {
        m_wsClients.removeAll(socket);
        socket->deleteLater();
        Logger::instance().info("LocalWebServer", QString("Cliente WebSocket desconectado (Restantes: %1)")
            .arg(m_wsClients.size()));
        emit clientCountChanged(m_wsClients.size());
    }
}

void LocalWebServer::onChatMessageReceived(const ChatMessage& msg, bool allowed)
{
    if (!m_isRunning || m_wsClients.isEmpty() || !allowed) return;

    QJsonObject obj = messageToJson(msg);
    obj["type"] = "chat_message";
    broadcastJson(obj);
}

void LocalWebServer::onViewerCountUpdated(const QString& platform, int count, int total)
{
    if (!m_isRunning || m_wsClients.isEmpty()) return;

    QJsonObject obj;
    obj["type"] = "viewer_count";
    obj["platform"] = platform;
    obj["count"] = count;
    obj["total"] = total;
    broadcastJson(obj);
}

void LocalWebServer::onPlatformStatusChanged(const QString& platform, PlatformStatus status, const QString& detail)
{
    if (!m_isRunning || m_wsClients.isEmpty()) return;

    QJsonObject obj;
    obj["type"] = "platform_status";
    obj["platform"] = platform;
    obj["status"] = platformStatusToString(status);
    obj["detail"] = detail;
    broadcastJson(obj);
}

void LocalWebServer::broadcastJson(const QJsonObject& json)
{
    QString msgStr = QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
    for (QWebSocket* client : m_wsClients)
    {
        if (client && client->isValid())
        {
            client->sendTextMessage(msgStr);
        }
    }
}

QJsonObject LocalWebServer::messageToJson(const ChatMessage& msg) const
{
    QJsonObject obj;
    obj["id"] = msg.id;
    obj["platform"] = msg.platform;
    obj["username"] = msg.username;
    obj["displayName"] = msg.displayName;
    obj["message"] = msg.message;
    obj["timestamp"] = msg.timestamp.toString(Qt::ISODate);
    obj["formattedTime"] = msg.formattedTime();
    obj["userColor"] = msg.userColor.isEmpty() ? "#9CA3AF" : msg.userColor;
    obj["type"] = static_cast<int>(msg.type);
    obj["isDonation"] = msg.isDonation;
    obj["donationAmount"] = msg.donationAmount;
    obj["currency"] = msg.currency;
    obj["giftName"] = msg.giftName;
    obj["giftCount"] = msg.giftCount;
    obj["isTransient"] = msg.isTransient;

    QJsonArray badges;
    for (const auto& b : msg.badges) badges.append(b);
    obj["badges"] = badges;

    QJsonArray emotes;
    for (const auto& e : msg.emotes)
    {
        QJsonObject eObj;
        eObj["start"] = e.start;
        eObj["length"] = e.length;
        eObj["imageUrl"] = e.imageUrl;
        eObj["altText"] = e.altText;
        emotes.append(eObj);
    }
    obj["emotes"] = emotes;
    return obj;
}

QByteArray LocalWebServer::getIndexHtml() const
{
    // The embedded HTML5/CSS3/JavaScript responsive client
    const QString html = QString(R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>MultiChat Live • Chat Unificado</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-primary: #0A0D14;
      --bg-surface: #121622;
      --bg-card: rgba(23, 28, 42, 0.7);
      --border-subtle: rgba(255, 255, 255, 0.08);
      --border-card: rgba(255, 255, 255, 0.06);
      --text-main: #F3F4F6;
      --text-muted: #94A3B8;
      --twitch: #9146FF;
      --youtube: #EF4444;
      --kick: #53FC18;
      --tiktok: #FE2C55;
      --accent: #6366F1;
      --success: #10B981;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      -webkit-tap-highlight-color: transparent;
    }

    body {
      font-family: 'Outfit', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg-primary);
      color: var(--text-main);
      height: 100vh;
      display: flex;
      flex-direction: column;
      overflow: hidden;
      font-size: 15px;
      line-height: 1.45;
    }

    /* OBS Overlay Mode (activated via ?overlay=true) */
    body.overlay-mode {
      background: transparent !important;
    }
    body.overlay-mode header,
    body.overlay-mode .controls-bar {
      display: none !important;
    }
    body.overlay-mode .chat-container {
      padding: 8px 12px;
      background: transparent;
    }
    body.overlay-mode .chat-msg {
      background: rgba(10, 13, 20, 0.85);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      border: 1px solid rgba(255, 255, 255, 0.12);
      box-shadow: 0 4px 14px rgba(0, 0, 0, 0.4);
    }

    /* Modern Header */
    header {
      background: var(--bg-surface);
      border-bottom: 1px solid var(--border-subtle);
      padding: 10px 16px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      flex-shrink: 0;
      z-index: 10;
    }

    .brand-wrap {
      display: flex;
      align-items: center;
      gap: 10px;
    }

    .brand-logo {
      width: 28px;
      height: 28px;
      background: linear-gradient(135deg, #6366F1, #8B5CF6);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 2px 8px rgba(99, 102, 241, 0.35);
    }
    .brand-logo svg {
      width: 16px;
      height: 16px;
      fill: #FFF;
    }

    .brand-title {
      font-weight: 700;
      font-size: 16px;
      letter-spacing: -0.3px;
      color: #FFF;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .live-badge {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      font-size: 11px;
      font-weight: 600;
      padding: 2px 7px;
      background: rgba(16, 185, 129, 0.15);
      color: var(--success);
      border: 1px solid rgba(16, 185, 129, 0.3);
      border-radius: 9999px;
    }
    .live-dot {
      width: 6px;
      height: 6px;
      background: var(--success);
      border-radius: 50%;
      animation: pulse 1.8s infinite;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(0.85); }
    }

    /* Platform chips bar */
    .platforms-bar {
      display: flex;
      align-items: center;
      gap: 6px;
      overflow-x: auto;
      scrollbar-width: none;
    }
    .platforms-bar::-webkit-scrollbar { display: none; }

    .platform-chip {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      padding: 4px 10px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: 600;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border-subtle);
      transition: all 0.2s;
    }
    .platform-chip.online {
      background: rgba(255, 255, 255, 0.08);
      border-color: rgba(255, 255, 255, 0.18);
    }
    .chip-dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: #64748B;
    }
    .platform-chip.twitch .chip-dot { background: var(--twitch); box-shadow: 0 0 6px var(--twitch); }
    .platform-chip.youtube .chip-dot { background: var(--youtube); box-shadow: 0 0 6px var(--youtube); }
    .platform-chip.kick .chip-dot { background: var(--kick); box-shadow: 0 0 6px var(--kick); }
    .platform-chip.tiktok .chip-dot { background: var(--tiktok); box-shadow: 0 0 6px var(--tiktok); }

    .header-actions {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .btn-icon {
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid var(--border-subtle);
      color: var(--text-main);
      padding: 6px 10px;
      border-radius: 8px;
      font-size: 12px;
      font-weight: 500;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.15s;
    }
    .btn-icon:hover {
      background: rgba(255, 255, 255, 0.12);
    }
    .btn-icon.active {
      background: var(--accent);
      border-color: var(--accent);
      color: #FFF;
    }

    /* Chat Messages Stream */
    .chat-container {
      flex: 1;
      overflow-y: auto;
      overflow-x: hidden;
      padding: 12px 16px;
      display: flex;
      flex-direction: column;
      gap: 8px;
      scroll-behavior: smooth;
    }

    .chat-msg {
      display: flex;
      flex-direction: column;
      background: var(--bg-card);
      border: 1px solid var(--border-card);
      border-radius: 10px;
      padding: 8px 12px;
      animation: slideIn 0.22s cubic-bezier(0.16, 1, 0.3, 1);
      word-break: break-word;
      transition: background 0.15s;
    }
    .chat-msg:hover {
      background: rgba(30, 36, 54, 0.85);
    }

    @keyframes slideIn {
      from { opacity: 0; transform: translateY(6px); }
      to { opacity: 1; transform: translateY(0); }
    }

    .msg-header {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-bottom: 3px;
      flex-wrap: wrap;
    }

    .badge-platform {
      font-size: 10px;
      font-weight: 700;
      text-transform: uppercase;
      padding: 2px 6px;
      border-radius: 5px;
      letter-spacing: 0.3px;
      display: inline-flex;
      align-items: center;
      gap: 4px;
    }
    .badge-platform.twitch { background: rgba(145, 70, 255, 0.2); color: #C084FC; border: 1px solid rgba(145, 70, 255, 0.35); }
    .badge-platform.youtube { background: rgba(239, 68, 68, 0.2); color: #F87171; border: 1px solid rgba(239, 68, 68, 0.35); }
    .badge-platform.kick { background: rgba(83, 252, 24, 0.2); color: #86EFAC; border: 1px solid rgba(83, 252, 24, 0.35); }
    .badge-platform.tiktok { background: rgba(254, 44, 85, 0.2); color: #FDA4AF; border: 1px solid rgba(254, 44, 85, 0.35); }

    .msg-time {
      font-family: 'JetBrains Mono', monospace;
      font-size: 11px;
      color: #64748B;
    }

    .msg-user {
      font-weight: 700;
      font-size: 14px;
      letter-spacing: -0.2px;
    }

    .msg-content {
      font-size: 14.5px;
      color: #E2E8F0;
      line-height: 1.4;
    }
    .msg-content img {
      height: 22px;
      vertical-align: middle;
      margin: 0 2px;
    }

    /* Highlight Card: Donations & Gifts */
    .chat-msg.donation {
      background: linear-gradient(135deg, rgba(245, 158, 11, 0.15), rgba(217, 119, 6, 0.08));
      border: 1px solid rgba(245, 158, 11, 0.4);
      border-left: 4px solid #F59E0B;
    }
    .chat-msg.gift {
      background: linear-gradient(135deg, rgba(236, 72, 153, 0.15), rgba(219, 39, 119, 0.08));
      border: 1px solid rgba(236, 72, 153, 0.4);
      border-left: 4px solid #EC4899;
    }
    .donation-pill {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      font-weight: 700;
      font-size: 13px;
      color: #FCD34D;
      background: rgba(245, 158, 11, 0.2);
      padding: 2px 8px;
      border-radius: 9999px;
      margin-right: 6px;
    }

    /* Transient Notifications (Likes, Joins, Follows) */
    .transient-toast {
      background: rgba(30, 41, 59, 0.9);
      border: 1px solid var(--border-subtle);
      border-left: 4px solid var(--accent);
      border-radius: 8px;
      padding: 6px 12px;
      font-size: 12.5px;
      color: #CBD5E1;
      display: flex;
      align-items: center;
      gap: 8px;
      animation: fadeInOut 4s forwards;
    }
    @keyframes fadeInOut {
      0% { opacity: 0; transform: translateY(-4px); }
      8% { opacity: 1; transform: translateY(0); }
      88% { opacity: 1; transform: translateY(0); }
      100% { opacity: 0; transform: translateY(-4px); }
    }

    /* Scroll button pill */
    .scroll-btn {
      position: absolute;
      bottom: 16px;
      right: 16px;
      background: #4F46E5;
      color: #FFF;
      padding: 8px 14px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: 600;
      border: none;
      box-shadow: 0 4px 12px rgba(79, 70, 229, 0.4);
      cursor: pointer;
      display: none;
      align-items: center;
      gap: 6px;
      z-index: 20;
    }
    .scroll-btn.visible {
      display: inline-flex;
    }
  </style>
</head>
<body>
  <header>
    <div class="brand-wrap">
      <div class="brand-logo">
        <svg viewBox="0 0 24 24"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>
      </div>
      <div>
        <div class="brand-title">
          MULTICHAT
          <span class="live-badge" id="liveBadge"><span class="live-dot"></span> EN VIVO</span>
        </div>
      </div>
    </div>

    <div class="platforms-bar">
      <div class="platform-chip twitch" id="chipTwitch"><span class="chip-dot"></span> Twitch: <span id="vTwitch">--</span></div>
      <div class="platform-chip youtube" id="chipYouTube"><span class="chip-dot"></span> YouTube: <span id="vYouTube">--</span></div>
      <div class="platform-chip kick" id="chipKick"><span class="chip-dot"></span> Kick: <span id="vKick">--</span></div>
      <div class="platform-chip tiktok" id="chipTikTok"><span class="chip-dot"></span> TikTok: <span id="vTikTok">--</span></div>
    </div>

    <div class="header-actions">
      <button class="btn-icon active" id="btnAutoScroll" title="Auto-scroll activado">⬇ Scroll</button>
      <button class="btn-icon" id="btnOverlayMode" title="Alternar modo OBS Overlay">OBS</button>
    </div>
  </header>

  <div class="chat-container" id="chatContainer"></div>
  <button class="scroll-btn" id="btnScrollBottom">⬇ Nuevos mensajes</button>

  <script>
    const isOverlay = new URLSearchParams(window.location.search).get('overlay') === 'true' || window.location.pathname.includes('/overlay');
    if (isOverlay) {
      document.body.classList.add('overlay-mode');
    }

    const container = document.getElementById('chatContainer');
    const scrollBottomBtn = document.getElementById('btnScrollBottom');
    const btnAutoScroll = document.getElementById('btnAutoScroll');
    const btnOverlayMode = document.getElementById('btnOverlayMode');
    let autoScroll = true;

    // Check auto-scroll toggle
    btnAutoScroll.addEventListener('click', () => {
      autoScroll = !autoScroll;
      btnAutoScroll.classList.toggle('active', autoScroll);
      if (autoScroll) scrollToBottom();
    });

    btnOverlayMode.addEventListener('click', () => {
      const url = new URL(window.location.href);
      if (document.body.classList.contains('overlay-mode')) {
        url.searchParams.delete('overlay');
      } else {
        url.searchParams.set('overlay', 'true');
      }
      window.location.href = url.toString();
    });

    scrollBottomBtn.addEventListener('click', () => {
      scrollToBottom();
      scrollBottomBtn.classList.remove('visible');
    });

    container.addEventListener('scroll', () => {
      const isAtBottom = container.scrollHeight - container.scrollTop <= container.clientHeight + 40;
      if (!isAtBottom) {
        scrollBottomBtn.classList.add('visible');
      } else {
        scrollBottomBtn.classList.remove('visible');
      }
    });

    function scrollToBottom() {
      container.scrollTop = container.scrollHeight;
    }

    // Connect WebSocket
    const wsPort = %1;
    const wsHost = window.location.hostname || 'localhost';
    const wsUrl = `ws://${wsHost}:${wsPort}`;
    let ws = null;

    function connectWs() {
      ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        console.log('MultiChat WebSocket conectado');
        const badge = document.getElementById('liveBadge');
        if (badge) badge.style.opacity = '1';
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          handleIncomingData(data);
        } catch (e) {
          console.error(e);
        }
      };

      ws.onclose = () => {
        console.warn('WebSocket desconectado. Reconectando en 2.5s...');
        const badge = document.getElementById('liveBadge');
        if (badge) badge.style.opacity = '0.4';
        setTimeout(connectWs, 2500);
      };

      ws.onerror = () => {
        ws.close();
      };
    }

    function handleIncomingData(data) {
      if (data.type === 'init') {
        if (data.recentMessages && Array.isArray(data.recentMessages)) {
          data.recentMessages.forEach(msg => appendMessage(msg, false));
          scrollToBottom();
        }
        if (data.viewers) updateViewerChips(data.viewers);
      } else if (data.type === 'chat_message') {
        appendMessage(data, true);
      } else if (data.type === 'viewer_count') {
        const update = {};
        update[data.platform] = data.count;
        updateViewerChips(update);
      }
    }

    function updateViewerChips(viewers) {
      for (const [platform, count] of Object.entries(viewers)) {
        const el = document.getElementById(`v${platform}`);
        const chip = document.getElementById(`chip${platform}`);
        if (el) el.textContent = count >= 0 ? count.toLocaleString() : '--';
        if (chip) chip.classList.toggle('online', count >= 0);
      }
    }

    function appendMessage(msg, animate) {
      // If transient event (like/follow/join), render as floating toast
      if (msg.isTransient) {
        renderTransientToast(msg);
        return;
      }

      const div = document.createElement('div');
      div.className = 'chat-msg';
      if (msg.isDonation) div.classList.add('donation');
      if (msg.type === 2) div.classList.add('gift');

      const platformClass = (msg.platform || 'kick').toLowerCase();
      const userColor = msg.userColor || '#818CF8';
      const time = msg.formattedTime || '';

      let donationBadge = '';
      if (msg.isDonation) {
        donationBadge = `<span class="donation-pill">🎁 ${msg.donationAmount > 0 ? (msg.donationAmount + ' ' + (msg.currency || '')) : ''}</span>`;
      }

      div.innerHTML = `
        <div class="msg-header">
          <span class="badge-platform ${platformClass}">${msg.platform}</span>
          <span class="msg-user" style="color: ${userColor}">${escapeHtml(msg.displayName || msg.username)}</span>
          <span class="msg-time">${time}</span>
        </div>
        <div class="msg-content">
          ${donationBadge}${formatMessageText(msg.message, msg.emotes)}
        </div>
      `;

      container.appendChild(div);

      // Keep max 100 messages in DOM for performance
      while (container.children.length > 100) {
        container.removeChild(container.firstChild);
      }

      if (autoScroll) {
        scrollToBottom();
      }
    }

    function renderTransientToast(msg) {
      const toast = document.createElement('div');
      toast.className = 'transient-toast';
      toast.innerHTML = `<span>👤</span> <strong>${escapeHtml(msg.displayName)}:</strong> ${escapeHtml(msg.message)}`;
      container.appendChild(toast);
      setTimeout(() => { toast.remove(); }, 4000);
      if (autoScroll) scrollToBottom();
    }

    function escapeHtml(str) {
      if (!str) return '';
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
    }

    function formatMessageText(text, emotes) {
      let safe = escapeHtml(text || '');
      if (emotes && Array.isArray(emotes) && emotes.length > 0) {
        emotes.forEach(e => {
          if (e.altText && e.imageUrl) {
            safe = safe.split(e.altText).join(`<img src="${e.imageUrl}" alt="${e.altText}" title="${e.altText}" />`);
          }
        });
      }
      return safe;
    }

    connectWs();
  </script>
</body>
</html>)HTML").arg(m_wsPort);

    return html.toUtf8();
}
