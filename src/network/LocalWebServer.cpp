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
      --border-subtle: rgba(255, 255, 255, 0.08);
      --text-main: #F3F4F6;
      --text-muted: #94A3B8;
      --twitch: #9146FF;
      --youtube: #EF4444;
      --kick: #53FC18;
      --tiktok: #FE2C55;
      --accent: #3B82F6;
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
      font-size: 14.5px;
      line-height: 1.45;
    }

    /* OBS Overlay Mode (activated via ?overlay=true) */
    body.overlay-mode {
      background: transparent !important;
    }
    body.overlay-mode header {
      display: none !important;
    }
    body.overlay-mode .chat-container {
      padding: 6px 10px;
      background: transparent;
    }
    body.overlay-mode .chat-row {
      text-shadow: 0 1px 3px rgba(0, 0, 0, 0.9), 0 2px 6px rgba(0, 0, 0, 0.7);
    }
    body.overlay-mode .chat-row.highlighted {
      background: rgba(10, 13, 20, 0.85);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
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
      background: linear-gradient(135deg, #2563EB, #3B82F6);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 2px 8px rgba(37, 99, 235, 0.35);
    }
    .brand-logo svg {
      width: 16px;
      height: 16px;
      fill: #FFF;
    }

    .brand-title {
      font-weight: 700;
      font-size: 15px;
      letter-spacing: -0.2px;
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
      background: #2563EB;
      border-color: #2563EB;
      color: #FFF;
    }
    .btn-icon svg {
      width: 14px;
      height: 14px;
      fill: currentColor;
    }

    /* Chat Messages Stream (Inline App Style) */
    .chat-container {
      flex: 1;
      overflow-y: auto;
      overflow-x: hidden;
      padding: 10px 14px 40px 14px;
      display: flex;
      flex-direction: column;
      gap: 3px;
      scroll-behavior: smooth;
    }

    .chat-row {
      display: block;
      padding: 2px 4px;
      line-height: 1.5;
      word-break: break-word;
      font-size: 14px;
      animation: fadeIn 0.15s ease-out;
      border-radius: 4px;
    }
    .chat-row:hover {
      background: rgba(255, 255, 255, 0.02);
    }

    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(3px); }
      to { opacity: 1; transform: translateY(0); }
    }

    .msg-time {
      font-family: 'JetBrains Mono', monospace;
      font-size: 11px;
      color: #64748B;
      margin-right: 6px;
    }

    .badge-platform {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: 18px;
      height: 18px;
      vertical-align: middle;
      margin-right: 6px;
      border-radius: 4px;
      background: rgba(255, 255, 255, 0.05);
      padding: 1px;
    }
    .badge-platform svg {
      width: 13px;
      height: 13px;
      display: block;
    }

    .msg-user {
      font-weight: 700;
      margin-right: 6px;
      letter-spacing: -0.1px;
    }

    .msg-body {
      color: #E2E8F0;
    }
    .msg-body img {
      height: 20px;
      vertical-align: middle;
      margin: 0 2px;
    }

    /* Highlighted Messages (Donations, Subs, Gifts) */
    .chat-row.donation {
      background: rgba(245, 158, 11, 0.12);
      border-left: 3px solid #F59E0B;
      padding: 5px 8px;
      margin: 3px 0;
      border-radius: 4px;
    }
    .chat-row.subscription {
      background: rgba(145, 70, 255, 0.12);
      border-left: 3px solid #9146FF;
      padding: 5px 8px;
      margin: 3px 0;
      border-radius: 4px;
    }
    .chat-row.gift {
      background: rgba(254, 44, 85, 0.12);
      border-left: 3px solid #FE2C55;
      padding: 5px 8px;
      margin: 3px 0;
      border-radius: 4px;
    }

    .pill-highlight {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      font-weight: 700;
      font-size: 12.5px;
      padding: 1px 6px;
      border-radius: 4px;
      margin-right: 6px;
      vertical-align: middle;
    }
    .pill-highlight svg {
      width: 13px;
      height: 13px;
      fill: currentColor;
    }
    .pill-highlight.donation {
      color: #FBBF24;
      background: rgba(245, 158, 11, 0.2);
    }
    .pill-highlight.subscription {
      color: #C084FC;
      background: rgba(145, 70, 255, 0.2);
    }
    .pill-highlight.gift {
      color: #FDA4AF;
      background: rgba(254, 44, 85, 0.2);
    }

    /* Dedicated Bottom Live Events Dock */
    .events-dock {
      position: fixed;
      bottom: 12px;
      left: 14px;
      right: 14px;
      display: flex;
      flex-direction: column-reverse;
      gap: 5px;
      pointer-events: none;
      z-index: 100;
    }

    .event-toast {
      align-self: flex-start;
      background: rgba(22, 27, 38, 0.96);
      border: 1px solid rgba(255, 255, 255, 0.12);
      border-radius: 9999px;
      padding: 4px 12px;
      display: inline-flex;
      align-items: center;
      gap: 7px;
      font-size: 12.5px;
      color: #E2E8F0;
      box-shadow: 0 4px 14px rgba(0, 0, 0, 0.45);
      animation: slideUpFade 3.8s cubic-bezier(0.16, 1, 0.3, 1) forwards;
      pointer-events: auto;
    }
    .event-toast svg {
      width: 14px;
      height: 14px;
      flex-shrink: 0;
      fill: currentColor;
    }
    .event-toast.join { border-left: 3px solid #38BDF8; color: #E0F2FE; }
    .event-toast.join svg { fill: #38BDF8; }
    .event-toast.share { border-left: 3px solid #34D399; color: #D1FAE5; }
    .event-toast.share svg { fill: #34D399; }
    .event-toast.follow { border-left: 3px solid #F472B6; color: #FCE7F3; }
    .event-toast.follow svg { fill: #F472B6; }
    .event-toast.like { border-left: 3px solid #F87171; color: #FEE2E2; }
    .event-toast.like svg { fill: #F87171; }

    @keyframes slideUpFade {
      0% { opacity: 0; transform: translateY(12px) scale(0.96); }
      10% { opacity: 1; transform: translateY(0) scale(1); }
      85% { opacity: 1; transform: translateY(0); }
      100% { opacity: 0; transform: translateY(-8px); }
    }

    /* Scroll button pill */
    .scroll-btn {
      position: absolute;
      bottom: 20px;
      right: 20px;
      background: #2563EB;
      color: #FFF;
      padding: 7px 12px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: 600;
      border: none;
      box-shadow: 0 4px 12px rgba(37, 99, 235, 0.45);
      cursor: pointer;
      display: none;
      align-items: center;
      gap: 6px;
      z-index: 20;
    }
    .scroll-btn svg {
      width: 14px;
      height: 14px;
      fill: currentColor;
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
      <button class="btn-icon active" id="btnAutoScroll" title="Auto-scroll activado">
        <svg viewBox="0 0 24 24"><path d="M11 4h2v12l4-4 1.4 1.4L12 20l-6.4-6.6L7 12l4 4V4z"/></svg>
        Scroll
      </button>
      <button class="btn-icon" id="btnOverlayMode" title="Alternar modo OBS Overlay">OBS</button>
    </div>
  </header>

  <div class="chat-container" id="chatContainer"></div>
  <div class="events-dock" id="eventsDock"></div>
  <button class="scroll-btn" id="btnScrollBottom">
    <svg viewBox="0 0 24 24"><path d="M11 4h2v12l4-4 1.4 1.4L12 20l-6.4-6.6L7 12l4 4V4z"/></svg>
    Nuevos mensajes
  </button>

  <script>
    const isOverlay = new URLSearchParams(window.location.search).get('overlay') === 'true' || window.location.pathname.includes('/overlay');
    if (isOverlay) {
      document.body.classList.add('overlay-mode');
    }

    const container = document.getElementById('chatContainer');
    const eventsDock = document.getElementById('eventsDock');
    const scrollBottomBtn = document.getElementById('btnScrollBottom');
    const btnAutoScroll = document.getElementById('btnAutoScroll');
    const btnOverlayMode = document.getElementById('btnOverlayMode');
    let autoScroll = true;

    // Platform SVG icons
    const platformSvgs = {
      twitch: `<svg viewBox="0 0 24 24"><path fill="#9146FF" d="M11.571 4.714h1.715v5.143H11.57zm4.715 0H18v5.143h-1.714zM6 0L1.714 4.286v15.428h5.143V24l4.286-4.286h3.428L22.286 12V0zm14.571 11.143l-3.428 3.428h-3.429l-3 3v-3H6.857V1.714h13.714Z"/></svg>`,
      youtube: `<svg viewBox="0 0 24 24"><path fill="#EF4444" d="M23.498 6.186a3.016 3.016 0 0 0-2.122-2.136C19.505 3.545 12 3.545 12 3.545s-7.505 0-9.377.505A3.017 3.017 0 0 0 .502 6.186C0 8.07 0 12 0 12s0 3.93.502 5.814a3.016 3.016 0 0 0 2.122 2.136c1.871.505 9.376.505 9.376.505s7.505 0 9.377-.505a3.015 3.015 0 0 0 2.122-2.136C24 15.93 24 12 24 12s0-3.93-.502-5.814zM9.545 15.568V8.432L15.818 12l-6.273 3.568z"/></svg>`,
      kick: `<svg viewBox="0 0 24 24"><path fill="#53FC18" d="M4 3h5v5h2V5h2V3h7v6h-3v2h-2v2h2v2h3v6h-7v-2h-2v-3h-2v5H4V3z"/></svg>`,
      tiktok: `<svg viewBox="0 0 24 24"><path fill="#FE2C55" d="M12.525.02c1.31-.02 2.61-.01 3.91-.02.08 1.53.63 3.09 1.75 4.17 1.12 1.11 2.7 1.62 4.24 1.79v4.03c-1.44-.05-2.89-.35-4.2-.97-.57-.26-1.1-.59-1.62-1.01-.01 2.92.01 5.84-.02 8.75-.08 1.4-.54 2.79-1.35 3.94-1.31 1.92-3.58 3.17-5.91 3.21-1.43.08-2.86-.31-4.08-1.03-2.02-1.19-3.44-3.37-3.65-5.71-.02-.5-.03-1-.01-1.49.18-1.9 1.12-3.72 2.58-4.96 1.66-1.44 3.98-2.13 6.15-1.72.02 1.48-.04 2.96-.04 4.44-.99-.32-2.15-.23-3.02.37-.63.41-1.11 1.04-1.36 1.75-.21.51-.24 1.07-.14 1.61.24 1.64 1.82 3.02 3.5 2.87 1.12-.01 2.19-.66 2.77-1.61.19-.33.4-.67.41-1.06.1-1.79.06-3.57.07-5.36.01-4.03-.01-8.05.02-12.07z"/></svg>`
    };

    const actionSvgs = {
      user: `<svg viewBox="0 0 24 24"><path d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0 2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"/></svg>`,
      gift: `<svg viewBox="0 0 24 24"><path d="M20 6h-2.18c.11-.31.18-.65.18-1 0-1.66-1.34-3-3-3-1.05 0-1.97.54-2.5 1.35l-.5.7-.5-.7C10.97 2.54 10.05 2 9 2 7.34 2 6 3.34 6 5c0 .35.07.69.18 1H4c-1.11 0-1.99.89-1.99 2L2 19c0 1.11.89 2 2 2h16c1.11 0 2-.89 2-2V8c0-1.11-.89-2-2-2zm-5-2c.55 0 1 .45 1 1s-.45 1-1 1h-2V5c0-.55.45-1 1-1zM9 4c.55 0 1 .45 1 1v1H8c0-.55.45-1 1-1zm11 15H4v-2h16v2zm0-5H4V8h7v6h2V8h7v6z"/></svg>`,
      star: `<svg viewBox="0 0 24 24"><path d="M12 2l3.09 6.26L22 9.27l-5 4.87 1.18 6.88L12 17.77l-6.18 3.25L7 14.14 2 9.27l6.91-1.01L12 2z"/></svg>`,
      heart: `<svg viewBox="0 0 24 24"><path d="M12 21.35l-1.45-1.32C5.4 15.36 2 12.28 2 8.5 2 5.42 4.42 3 7.5 3c1.74 0 3.41.81 4.5 2.09C13.09 3.81 14.76 3 16.5 3 19.58 3 22 5.42 22 8.5c0 3.78-3.4 6.86-8.55 11.54L12 21.35z"/></svg>`,
      share: `<svg viewBox="0 0 24 24"><path d="M18 16.08c-.76 0-1.44.3-1.96.77L8.91 12.7c.05-.23.09-.46.09-.7s-.04-.47-.09-.7l7.05-4.11c.54.5 1.25.81 2.04.81 1.66 0 3-1.34 3-3s-1.34-3-3-3-3 1.34-3 3c0 .24.04.47.09.7L8.04 9.81C7.5 9.31 6.79 9 6 9c-1.66 0-3 1.34-3 3s1.34 3 3 3c.79 0 1.5-.31 2.04-.81l7.12 4.16c-.05.21-.08.43-.08.65 0 1.61 1.31 2.92 2.92 2.92 1.61 0 2.92-1.31 2.92-2.92s-1.31-2.92-2.92-2.92z"/></svg>`
    };

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
      // If transient event (like/follow/join/share), render in bottom dock
      if (msg.isTransient) {
        renderTransientToast(msg);
        return;
      }

      const row = document.createElement('div');
      row.className = 'chat-row';

      const platformKey = (msg.platform || 'kick').toLowerCase();
      const platformIcon = platformSvgs[platformKey] || platformSvgs.kick;
      const userColor = msg.userColor || '#818CF8';
      const time = msg.formattedTime || '';

      let highlightBadge = '';
      if (msg.isDonation || msg.type === 1) {
        row.classList.add('donation', 'highlighted');
        const amountStr = msg.donationAmount > 0 ? `${msg.donationAmount.toFixed(2)} ${msg.currency || ''}` : '';
        highlightBadge = `<span class="pill-highlight donation">${actionSvgs.gift} [${amountStr}]</span> `;
      } else if (msg.type === 2) {
        row.classList.add('gift', 'highlighted');
        highlightBadge = `<span class="pill-highlight gift">${actionSvgs.gift}</span> `;
      } else if (msg.type === 3) {
        row.classList.add('subscription', 'highlighted');
        highlightBadge = `<span class="pill-highlight subscription">${actionSvgs.star}</span> `;
      }

      row.innerHTML = `
        <span class="msg-time">[${time}]</span><span class="badge-platform ${platformKey}">${platformIcon}</span><span class="msg-user" style="color: ${userColor}">${escapeHtml(msg.displayName || msg.username)}:</span><span class="msg-body">${highlightBadge}${formatMessageText(msg.message, msg.emotes)}</span>
      `;

      container.appendChild(row);

      // Keep max 120 messages in DOM
      while (container.children.length > 120) {
        container.removeChild(container.firstChild);
      }

      if (autoScroll) {
        scrollToBottom();
      }
    }

    function renderTransientToast(msg) {
      const toast = document.createElement('div');
      let typeClass = 'join';
      let svgIcon = actionSvgs.user;

      if (msg.type === 5) { // LIKE
        typeClass = 'like';
        svgIcon = actionSvgs.heart;
      } else if (msg.type === 6) { // SHARE
        typeClass = 'share';
        svgIcon = actionSvgs.share;
      } else if (msg.type === 7) { // FOLLOW
        typeClass = 'follow';
        svgIcon = actionSvgs.star;
      }

      toast.className = `event-toast ${typeClass}`;
      toast.innerHTML = `${svgIcon} <strong>${escapeHtml(msg.displayName)}:</strong> <span>${escapeHtml(msg.message)}</span>`;
      eventsDock.appendChild(toast);

      setTimeout(() => {
        if (toast.parentNode) toast.parentNode.removeChild(toast);
      }, 3800);
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
