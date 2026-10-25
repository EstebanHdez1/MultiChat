#include "TwitchPlatform.h"
#include "HttpHelper.h"
#include "Logger.h"
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

TwitchPlatform::TwitchPlatform(QObject* parent)
    : BasePlatform("Twitch", parent)
    , m_ws(new WebSocketWrapper("TwitchWS", this))
{
    connect(m_ws, &WebSocketWrapper::connected, this, &TwitchPlatform::onWsConnected);
    connect(m_ws, &WebSocketWrapper::disconnected, this, &TwitchPlatform::onWsDisconnected);
    connect(m_ws, &WebSocketWrapper::textMessageReceived, this, &TwitchPlatform::onWsTextMessage);

    connect(m_pollTimer, &QTimer::timeout, this, &TwitchPlatform::onPollStreamStatus);
    m_pollIntervalMs = 12000; // 12 seconds
}

TwitchPlatform::~TwitchPlatform()
{
    doDisconnect();
}

void TwitchPlatform::doConnect()
{
    m_currentChannelJoined = m_channelName.toLower().remove('#').remove('@').trimmed();
    if (m_currentChannelJoined.isEmpty())
    {
        setStatus(PlatformStatus::OFFLINE, "Canal de Twitch vacío");
        return;
    }

    m_ws->connectUrl(QUrl("wss://irc-ws.chat.twitch.tv:443"));
    m_pollTimer->start(m_pollIntervalMs);
    onPollStreamStatus();
}

void TwitchPlatform::doDisconnect()
{
    m_pollTimer->stop();
    m_ws->disconnectSocket();
    m_currentChannelJoined.clear();
}

void TwitchPlatform::startChatListener()
{
    if (!m_ws->isConnected())
    {
        doConnect();
    }
}

void TwitchPlatform::stopChatListener()
{
    doDisconnect();
}

void TwitchPlatform::onWsConnected()
{
    Logger::instance().info("Twitch", QString("Conectado a IRC Twitch para el canal: %1").arg(m_currentChannelJoined));
    
    int randomId = QRandomGenerator::global()->bounded(10000, 99999);
    QString nick = QString("justinfan%1").arg(randomId);

    m_ws->sendTextMessage("CAP REQ :twitch.tv/tags twitch.tv/commands");
    m_ws->sendTextMessage("PASS SCHMOOPIE");
    m_ws->sendTextMessage(QString("NICK %1").arg(nick));
    m_ws->sendTextMessage(QString("JOIN #%1").arg(m_currentChannelJoined));

    setStatus(PlatformStatus::ONLINE);
}

void TwitchPlatform::onWsDisconnected()
{
    // WebSocketWrapper handles auto-reconnect
}

void TwitchPlatform::onWsTextMessage(const QString& message)
{
    QStringList lines = message.split("\r\n", Qt::SkipEmptyParts);
    for (const QString& line : lines)
    {
        parseIrcLine(line.trimmed());
    }
}

void TwitchPlatform::parseIrcLine(const QString& line)
{
    if (line.isEmpty()) return;

    // Handle PING / PONG
    if (line.startsWith("PING"))
    {
        m_ws->sendTextMessage("PONG :tmi.twitch.tv");
        return;
    }

    QMap<QString, QString> tags;
    QString rest = line;

    if (rest.startsWith('@'))
    {
        int spaceIdx = rest.indexOf(' ');
        if (spaceIdx > 0)
        {
            QString rawTags = rest.mid(1, spaceIdx - 1);
            tags = parseTags(rawTags);
            rest = rest.mid(spaceIdx + 1).trimmed();
        }
    }

    QString prefix;
    if (rest.startsWith(':'))
    {
        int spaceIdx = rest.indexOf(' ');
        if (spaceIdx > 0)
        {
            prefix = rest.mid(1, spaceIdx - 1);
            rest = rest.mid(spaceIdx + 1).trimmed();
        }
    }

    int spaceIdx = rest.indexOf(' ');
    QString command = (spaceIdx > 0) ? rest.left(spaceIdx) : rest;
    QString params = (spaceIdx > 0) ? rest.mid(spaceIdx + 1).trimmed() : "";

    if (command == "PRIVMSG")
    {
        int colonIdx = params.indexOf(':');
        QString channel = (colonIdx > 0) ? params.left(colonIdx).trimmed() : params;
        QString text = (colonIdx >= 0) ? params.mid(colonIdx + 1) : "";
        parsePrivMsg(tags, prefix, channel, text);
    }
    else if (command == "USERNOTICE")
    {
        int colonIdx = params.indexOf(':');
        QString channel = (colonIdx > 0) ? params.left(colonIdx).trimmed() : params;
        QString userMsg = (colonIdx >= 0) ? params.mid(colonIdx + 1) : "";
        QString sysMsg = tags.value("system-msg", "");
        parseUserNotice(tags, channel, sysMsg, userMsg);
    }
    else if (command == "ROOMSTATE")
    {
        setStatus(PlatformStatus::ONLINE);
    }
}

QMap<QString, QString> TwitchPlatform::parseTags(const QString& rawTags)
{
    QMap<QString, QString> result;
    QStringList pairs = rawTags.split(';', Qt::SkipEmptyParts);
    for (const QString& pair : pairs)
    {
        int eq = pair.indexOf('=');
        if (eq > 0)
        {
            QString k = pair.left(eq);
            QString v = pair.mid(eq + 1);
            v.replace("\\s", " ");
            v.replace("\\:", ";");
            result[k] = v;
        }
    }
    return result;
}

void TwitchPlatform::parsePrivMsg(const QMap<QString, QString>& tags, const QString& prefix, const QString& channel, const QString& text)
{
    Q_UNUSED(channel);

    ChatMessage msg;
    msg.id = tags.value("id", QString::number(QDateTime::currentMSecsSinceEpoch()));
    msg.platform = "Twitch";

    QString displayName = tags.value("display-name");
    QString username = prefix.section('!', 0, 0);
    if (displayName.isEmpty()) displayName = username;

    msg.username = username;
    msg.displayName = displayName;
    msg.message = text;
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = tags.value("color", "#9146FF");
    if (msg.userColor.isEmpty()) msg.userColor = "#9146FF";

    // Badges
    QString rawBadges = tags.value("badges");
    if (!rawBadges.isEmpty())
    {
        QStringList bList = rawBadges.split(',');
        for (const QString& b : bList)
        {
            msg.badges.append(b.section('/', 0, 0));
        }
    }

    // Twitch IRC includes every native emote as id:start-end ranges. Keep the
    // original text for accessibility and let the chat view render the image.
    const QString rawEmotes = tags.value("emotes");
    for (const QString& emoteGroup : rawEmotes.split('/', Qt::SkipEmptyParts))
    {
        const QStringList parts = emoteGroup.split(':');
        if (parts.size() != 2) continue;
        for (const QString& range : parts[1].split(',', Qt::SkipEmptyParts))
        {
            const QStringList bounds = range.split('-');
            if (bounds.size() != 2) continue;
            const int start = bounds[0].toInt();
            const int end = bounds[1].toInt();
            if (start < 0 || end < start || end >= text.size()) continue;

            ChatEmote emote;
            emote.start = start;
            emote.length = end - start + 1;
            emote.altText = text.mid(start, emote.length);
            emote.imageUrl = QString("https://static-cdn.jtvnw.net/emoticons/v2/%1/default/dark/1.0").arg(parts[0]);
            msg.emotes.append(emote);
        }
    }

    // Check for Cheers / Bits
    if (tags.contains("bits"))
    {
        int bits = tags.value("bits").toInt();
        if (bits > 0)
        {
            msg.type = MessageType::DONATION;
            msg.isDonation = true;
            msg.donationAmount = bits;
            msg.currency = "Bits";

            Donation don;
            don.id = msg.id;
            don.platform = "Twitch";
            don.username = msg.username;
            don.displayName = msg.displayName;
            don.amount = bits;
            don.currency = "Bits";
            don.type = "Cheer / Bits";
            don.message = text;
            don.timestamp = msg.timestamp;
            emitDonation(don);
        }
    }

    emitChatMessage(msg);
}

void TwitchPlatform::parseUserNotice(const QMap<QString, QString>& tags, const QString& channel, const QString& sysMsg, const QString& userMsg)
{
    Q_UNUSED(channel);

    QString msgId = tags.value("msg-id");
    QString displayName = tags.value("display-name");
    QString login = tags.value("login");
    if (displayName.isEmpty()) displayName = login;

    ChatMessage msg;
    msg.id = tags.value("id", QString::number(QDateTime::currentMSecsSinceEpoch()));
    msg.platform = "Twitch";
    msg.username = login;
    msg.displayName = displayName;
    msg.userColor = tags.value("color", "#9146FF");
    msg.timestamp = QDateTime::currentDateTime();

    if (msgId.contains("sub", Qt::CaseInsensitive) || msgId.contains("gift", Qt::CaseInsensitive) || msgId.contains("prime", Qt::CaseInsensitive))
    {
        msg.type = msgId.contains("gift") ? MessageType::GIFT : MessageType::SUBSCRIPTION;
        msg.isDonation = true;
        msg.donationAmount = 4.99;
        msg.currency = "$";
        msg.message = sysMsg.isEmpty() ? (msgId.contains("gift") ? "Regaló una suscripción!" : "Se ha suscrito al canal!") : sysMsg;
        if (!userMsg.isEmpty())
        {
            msg.message += " - " + userMsg;
        }

        Donation don;
        don.id = msg.id;
        don.platform = "Twitch";
        don.username = login;
        don.displayName = displayName;
        don.amount = 4.99;
        don.currency = "$";
        don.type = msgId.contains("gift") ? "Twitch Gift Sub" : "Twitch Subscription";
        don.message = msg.message;
        don.timestamp = msg.timestamp;
        emitDonation(don);
    }
    else
    {
        msg.type = MessageType::NORMAL;
        msg.message = sysMsg.isEmpty() ? userMsg : sysMsg;
    }

    emitChatMessage(msg);
}

void TwitchPlatform::onPollStreamStatus()
{
    if (m_currentChannelJoined.isEmpty()) return;

    // Method 1: Twitch GQL
    QString url = "https://gql.twitch.tv/gql";
    QMap<QString, QString> headers;
    headers["Client-Id"] = "kimne78kx3ncx6brgo4mv6wki5h1ko";
    headers["Content-Type"] = "application/json";

    QJsonObject queryObj;
    queryObj["query"] = QString("query { user(login: \"%1\") { stream { viewersCount isEncrypted type } } }").arg(m_currentChannelJoined);
    QJsonDocument reqDoc(queryObj);

    HttpHelper::instance().post(url, headers, reqDoc.toJson(), [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        Q_UNUSED(statusCode);
        Q_UNUSED(err);
        if (success)
        {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject())
            {
                QJsonObject root = doc.object();
                QJsonObject dataObj = root["data"].toObject();
                QJsonObject userObj = dataObj["user"].toObject();
                
                if (userObj.contains("stream") && userObj["stream"].isObject())
                {
                    QJsonObject streamObj = userObj["stream"].toObject();
                    int viewers = streamObj["viewersCount"].toInt(-1);
                    if (viewers >= 0)
                    {
                        setViewerCount(viewers);
                        setStatus(PlatformStatus::ONLINE);
                        return;
                    }
                }
            }
        }

        // Fallback: Check twitch HTML page directly
        QString pageUrl = QString("https://www.twitch.tv/%1").arg(m_currentChannelJoined);
        HttpHelper::instance().get(pageUrl, {}, [this](bool pageSuccess, int pageCode, const QByteArray& pageData, const QString& pageErr) {
            Q_UNUSED(pageCode);
            Q_UNUSED(pageErr);
            if (!pageSuccess) return;

            QString html = QString::fromUtf8(pageData);
            QRegularExpression vRegex("\"viewersCount\":([0-9]+)");
            auto match = vRegex.match(html);
            if (match.hasMatch())
            {
                int viewers = match.captured(1).toInt();
                setViewerCount(viewers);
                setStatus(PlatformStatus::ONLINE);
            }
            else if (html.contains("\"isLiveBroadcast\":true"))
            {
                setStatus(PlatformStatus::ONLINE);
            }
        });
    });
}
