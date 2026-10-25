#include "KickPlatform.h"
#include "HttpHelper.h"
#include "Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>

KickPlatform::KickPlatform(QObject* parent)
    : BasePlatform("Kick", parent)
    , m_ws(new WebSocketWrapper("KickCentrifugoWS", this))
{
    connect(m_ws, &WebSocketWrapper::connected, this, &KickPlatform::onWsConnected);
    connect(m_ws, &WebSocketWrapper::disconnected, this, &KickPlatform::onWsDisconnected);
    connect(m_ws, &WebSocketWrapper::textMessageReceived, this, &KickPlatform::onWsTextMessage);

    connect(m_pollTimer, &QTimer::timeout, this, &KickPlatform::onPollStreamStatus);
    m_pollIntervalMs = 12000;
}

KickPlatform::~KickPlatform()
{
    doDisconnect();
}

void KickPlatform::doConnect()
{
    m_slug = m_channelName.toLower().remove('@').remove('#').trimmed();
    if (m_slug.isEmpty())
    {
        setStatus(PlatformStatus::OFFLINE, "Canal de Kick no especificado");
        return;
    }

    if (!m_extraKey.isEmpty() && m_extraKey.toLongLong() > 0)
    {
        m_chatroomId = m_extraKey.toLongLong();
        Logger::instance().info("Kick", QString("Usando chatroomId configurado: %1").arg(m_chatroomId));
        requestCentrifugoTokenAndConnect();
        return;
    }

    resolveChannelAndConnect();
}

void KickPlatform::doDisconnect()
{
    m_pollTimer->stop();
    m_ws->disconnectSocket();
    m_chatroomId = 0;
    m_kickChannelId = 0;
    m_centrifugoToken.clear();
}

void KickPlatform::startChatListener()
{
    if (!m_ws->isConnected())
    {
        doConnect();
    }
}

void KickPlatform::stopChatListener()
{
    doDisconnect();
}

void KickPlatform::resolveChannelAndConnect()
{
    setStatus(PlatformStatus::CONNECTING);

    // Method 1: Try Kick API v2
    QString url = QString("https://kick.com/api/v2/channels/%1").arg(m_slug);
    QMap<QString, QString> headers;
    headers["Accept"] = "application/json, text/plain, */*";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

    Logger::instance().info("Kick", QString("Resolviendo canal Kick: %1").arg(m_slug));

    HttpHelper::instance().get(url, headers, [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        if (success && (statusCode == 200 || statusCode == 0))
        {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject())
            {
                QJsonObject root = doc.object();
                m_kickChannelId = root["id"].toInteger();
                QJsonObject chatroom = root["chatroom"].toObject();
                m_chatroomId = chatroom["id"].toInteger();

                if (root.contains("livestream") && !root["livestream"].isNull())
                {
                    QJsonObject ls = root["livestream"].toObject();
                    int v = ls["viewer_count"].toInt(ls["viewers"].toInt(-1));
                    if (v >= 0) setViewerCount(v);
                }

                if (m_chatroomId > 0)
                {
                    Logger::instance().info("Kick", QString("Canal Kick resuelto exitosamente: id=%1, chatroomId=%2").arg(m_kickChannelId).arg(m_chatroomId));
                    requestCentrifugoTokenAndConnect();
                    return;
                }
            }
        }

        // Method 2: Scrape HTML page https://kick.com/<slug>
        QString pageUrl = QString("https://kick.com/%1").arg(m_slug);
        QMap<QString, QString> pHeaders;
        pHeaders["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";
        pHeaders["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

        HttpHelper::instance().get(pageUrl, pHeaders, [this](bool pSuccess, int pCode, const QByteArray& pData, const QString& pErr) {
            Q_UNUSED(pCode);
            Q_UNUSED(pErr);
            if (pSuccess)
            {
                QString html = QString::fromUtf8(pData);
                
                QRegularExpression crRegex("chatrooms\\.([0-9]+)\\.v2");
                auto match = crRegex.match(html);
                if (!match.hasMatch())
                {
                    crRegex = QRegularExpression("\"chatroom\":\\{\"id\":([0-9]+)");
                    match = crRegex.match(html);
                }
                if (!match.hasMatch())
                {
                    crRegex = QRegularExpression("\"chatroom_id\":([0-9]+)");
                    match = crRegex.match(html);
                }

                if (match.hasMatch())
                {
                    m_chatroomId = match.captured(1).toLongLong();
                    Logger::instance().info("Kick", QString("Canal Kick resuelto vía HTML: chatroomId=%1").arg(m_chatroomId));

                    QRegularExpression viewRegex("\"viewer_count\":([0-9]+)");
                    auto vMatch = viewRegex.match(html);
                    if (!vMatch.hasMatch())
                    {
                        viewRegex = QRegularExpression("\"viewers\":([0-9]+)");
                        vMatch = viewRegex.match(html);
                    }
                    if (vMatch.hasMatch())
                    {
                        setViewerCount(vMatch.captured(1).toInt());
                    }

                    requestCentrifugoTokenAndConnect();
                    return;
                }
            }

            Logger::instance().warn("Kick", "No se pudo resolver el canal automáticamente. Verifique el nombre.");
            setStatus(PlatformStatus::ERROR_STATE, "No se pudo resolver canal");
        });
    });
}

void KickPlatform::requestCentrifugoTokenAndConnect()
{
    QString authUrl = "https://web.kick.com/api/v1/realtime/auth/connection";
    QString clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonObject reqObj;
    reqObj["client_id"] = clientId;
    QByteArray body = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

    QMap<QString, QString> headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

    Logger::instance().info("Kick", QString("Solicitando token de autenticación realtime para Kick (client_id: %1)...").arg(clientId));

    HttpHelper::instance().post(authUrl, headers, body, [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        if (!success)
        {
            Logger::instance().error("Kick", QString("Error al obtener token de realtime (HTTP %1): %2").arg(statusCode).arg(err));
            setStatus(PlatformStatus::ERROR_STATE, "Error de autenticación WebSocket");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("data") && obj["data"].isObject())
            {
                m_centrifugoToken = obj["data"].toObject()["token"].toString();
            }
            else if (obj.contains("token"))
            {
                m_centrifugoToken = obj["token"].toString();
            }
        }

        if (m_centrifugoToken.isEmpty())
        {
            Logger::instance().error("Kick", QString("Respuesta de token vacía o inválida: %1").arg(QString::fromUtf8(data)));
            setStatus(PlatformStatus::ERROR_STATE, "Token inválido");
            return;
        }

        Logger::instance().info("Kick", "Token realtime obtenido. Conectando al WebSocket de Kick...");
        QUrl wsUrl("wss://realtime.us-west-2.platform.kick.com/connection/websocket");
        m_ws->connectUrl(wsUrl);
        m_pollTimer->start(m_pollIntervalMs);
    });
}

void KickPlatform::onWsConnected()
{
    Logger::instance().info("Kick", "Conectado al WebSocket de Kick. Autenticando sesión...");

    QJsonObject connectObj;
    QJsonObject connInner;
    connInner["token"] = m_centrifugoToken;
    connInner["name"] = "js";
    connectObj["connect"] = connInner;
    connectObj["id"] = 1;

    m_ws->sendTextMessage(QString::fromUtf8(QJsonDocument(connectObj).toJson(QJsonDocument::Compact)));
}

void KickPlatform::onWsDisconnected()
{
    // Handled by auto-reconnect
}

void KickPlatform::subscribeToCentrifugoChannels()
{
    if (m_chatroomId > 0)
    {
        QJsonObject subObj;
        QJsonObject subInner;
        subInner["channel"] = QString("chatrooms.%1.v2").arg(m_chatroomId);
        subInner["flag"] = 1;
        subObj["subscribe"] = subInner;
        subObj["id"] = 2;
        m_ws->sendTextMessage(QString::fromUtf8(QJsonDocument(subObj).toJson(QJsonDocument::Compact)));
        Logger::instance().info("Kick", QString("Suscrito a chat de Kick: chatrooms.%1.v2").arg(m_chatroomId));
    }

    if (m_kickChannelId > 0)
    {
        QJsonObject subObj;
        QJsonObject subInner;
        subInner["channel"] = QString("channel.%1").arg(m_kickChannelId);
        subInner["flag"] = 1;
        subObj["subscribe"] = subInner;
        subObj["id"] = 3;
        m_ws->sendTextMessage(QString::fromUtf8(QJsonDocument(subObj).toJson(QJsonDocument::Compact)));
        Logger::instance().info("Kick", QString("Suscrito a eventos de Kick: channel.%1").arg(m_kickChannelId));
    }
}

void KickPlatform::onWsTextMessage(const QString& message)
{
    QString trimmed = message.trimmed();
    if (trimmed.isEmpty() || trimmed == "{}")
    {
        // Centrifugo heartbeat ping -> reply pong
        m_ws->sendTextMessage("{}");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();

    // 1. Connection established response
    if (obj.contains("connect"))
    {
        Logger::instance().info("Kick", "Sesión iniciada exitosamente en Kick.");
        setStatus(PlatformStatus::ONLINE);
        subscribeToCentrifugoChannels();
        return;
    }

    // 2. Subscription confirmation
    if (obj.contains("subscribe"))
    {
        Logger::instance().info("Kick", "Suscripción confirmada en Kick.");
        return;
    }

    // 3. Ping handling
    if (obj.contains("ping"))
    {
        m_ws->sendTextMessage("{}");
        return;
    }

    // 4. Push event notification
    if (obj.contains("push"))
    {
        QJsonObject push = obj["push"].toObject();
        QJsonObject pub = push["pub"].toObject();
        QJsonValue dataVal = pub["data"];

        QJsonObject eventContainer;
        if (dataVal.isObject())
        {
            eventContainer = dataVal.toObject();
        }
        else if (dataVal.isString())
        {
            QJsonDocument pDoc = QJsonDocument::fromJson(dataVal.toString().toUtf8());
            if (pDoc.isObject()) eventContainer = pDoc.object();
        }

        if (!eventContainer.isEmpty())
        {
            QString event = eventContainer["event"].toString();
            QJsonObject dataObj;
            if (eventContainer["data"].isString())
            {
                QJsonDocument dataDoc = QJsonDocument::fromJson(eventContainer["data"].toString().toUtf8());
                if (dataDoc.isObject()) dataObj = dataDoc.object();
            }
            else if (eventContainer["data"].isObject())
            {
                dataObj = eventContainer["data"].toObject();
            }

            if (!dataObj.isEmpty())
            {
                parseKickEvent(event, dataObj);
            }
        }
        return;
    }

    // Fallback for legacy pusher-like format if any
    QString event = obj["event"].toString();
    if (!event.isEmpty())
    {
        if (event == "pusher:ping")
        {
            m_ws->sendTextMessage("{\"event\":\"pusher:pong\",\"data\":{}}");
            return;
        }

        QJsonObject dataObj;
        if (obj["data"].isString())
        {
            QJsonDocument dataDoc = QJsonDocument::fromJson(obj["data"].toString().toUtf8());
            if (dataDoc.isObject()) dataObj = dataDoc.object();
        }
        else if (obj["data"].isObject())
        {
            dataObj = obj["data"].toObject();
        }
        if (!dataObj.isEmpty())
        {
            parseKickEvent(event, dataObj);
        }
    }
}

void KickPlatform::parseKickEvent(const QString& eventName, const QJsonObject& dataObj)
{
    if (eventName.contains("ChatMessageEvent") || eventName.contains("ChatMessage") || eventName.contains("message", Qt::CaseInsensitive))
    {
        handleChatMessageEvent(dataObj);
    }
    else if (eventName.contains("SubscriptionEvent") || eventName.contains("Subscription", Qt::CaseInsensitive))
    {
        handleSubscriptionEvent(dataObj, false);
    }
    else if (eventName.contains("GiftedSubscriptionsEvent") || eventName.contains("Gift", Qt::CaseInsensitive))
    {
        handleSubscriptionEvent(dataObj, true);
    }
    else if (eventName.contains("StreamHostEvent"))
    {
        int viewers = dataObj["number_viewers"].toInt(dataObj["viewers"].toInt(-1));
        if (viewers >= 0) setViewerCount(viewers);
    }
}

void KickPlatform::handleChatMessageEvent(const QJsonObject& dataObj)
{
    ChatMessage msg;
    msg.id = dataObj["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    msg.platform = "Kick";

    // Kick sends custom emotes inline as [emote:<id>:<name>]. Keep their
    // position in the rendered text and load the public Kick CDN asset.
    const QString rawContent = dataObj["content"].toString();
    const QRegularExpression emoteRegex("\\[emote:([0-9]+):([^\\]]+)\\]");
    QString content;
    int sourcePosition = 0;
    auto match = emoteRegex.globalMatch(rawContent);
    while (match.hasNext())
    {
        const QRegularExpressionMatch current = match.next();
        content += rawContent.mid(sourcePosition, current.capturedStart() - sourcePosition);
        const QString name = current.captured(2).trimmed();
        const QString placeholder = QString(":%1:").arg(name);
        const int start = content.size();
        content += placeholder;
        msg.emotes.append({ start, static_cast<int>(placeholder.size()),
            QString("https://files.kick.com/emotes/%1/fullsize").arg(current.captured(1)), placeholder });
        sourcePosition = current.capturedEnd();
    }
    content += rawContent.mid(sourcePosition);
    msg.message = content;
    msg.timestamp = QDateTime::currentDateTime();

    QJsonObject sender = dataObj["sender"].toObject();
    msg.username = sender["username"].toString();
    msg.displayName = sender["username"].toString();

    QJsonObject identity = sender["identity"].toObject();
    msg.userColor = identity["color"].toString("#53FC18");
    if (msg.userColor.isEmpty()) msg.userColor = "#53FC18";

    QJsonArray badges = identity["badges"].toArray();
    for (const auto& b : badges)
    {
        QJsonObject bObj = b.toObject();
        msg.badges.append(bObj["type"].toString());
    }

    msg.type = MessageType::NORMAL;
    emitChatMessage(msg);
}

void KickPlatform::handleSubscriptionEvent(const QJsonObject& dataObj, bool isGift)
{
    QString username = dataObj["username"].toString(dataObj["gifter_username"].toString());
    int months = dataObj["months"].toInt(1);

    ChatMessage msg;
    msg.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    msg.platform = "Kick";
    msg.username = username;
    msg.displayName = username;
    msg.userColor = "#53FC18";
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = isGift ? MessageType::GIFT : MessageType::SUBSCRIPTION;
    msg.isDonation = true;
    msg.donationAmount = 5.00;
    msg.currency = "$";
    msg.message = isGift ? QString("¡Regaló una suscripción en Kick!") : QString("¡Se ha suscrito por %1 meses!").arg(months);

    Donation don;
    don.id = msg.id;
    don.platform = "Kick";
    don.username = username;
    don.displayName = username;
    don.amount = 5.00;
    don.currency = "$";
    don.type = isGift ? "Kick Gift Sub" : "Kick Subscription";
    don.message = msg.message;
    don.timestamp = msg.timestamp;

    emitDonation(don);
    emitChatMessage(msg);
}

void KickPlatform::onPollStreamStatus()
{
    if (m_slug.isEmpty()) return;

    QString url = QString("https://kick.com/api/v2/channels/%1").arg(m_slug);
    QMap<QString, QString> headers;
    headers["Accept"] = "application/json";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

    HttpHelper::instance().get(url, headers, [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        Q_UNUSED(statusCode);
        Q_UNUSED(err);
        if (!success) return;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return;

        QJsonObject root = doc.object();
        if (root.contains("livestream") && !root["livestream"].isNull())
        {
            QJsonObject ls = root["livestream"].toObject();
            int viewers = ls["viewer_count"].toInt(ls["viewers"].toInt(-1));
            if (viewers >= 0)
            {
                setViewerCount(viewers);
                setStatus(PlatformStatus::ONLINE);
            }
        }
        else
        {
            setViewerCount(-1);
        }
    });
}
