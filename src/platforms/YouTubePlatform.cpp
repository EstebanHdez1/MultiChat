#include "YouTubePlatform.h"
#include "HttpHelper.h"
#include "Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace
{
QString extractRunsText(const QJsonObject& container)
{
    QString text = container["simpleText"].toString();
    if (!text.isEmpty()) return text;
    for (const auto& r : container["runs"].toArray())
    {
        text += r.toObject()["text"].toString();
    }
    return text;
}

QString extractAuthorName(const QJsonObject& renderer)
{
    if (renderer.contains("authorName") && renderer["authorName"].isString())
    {
        return renderer["authorName"].toString();
    }
    return extractRunsText(renderer["authorName"].toObject());
}
}

YouTubePlatform::YouTubePlatform(QObject* parent)
    : BasePlatform("YouTube", parent)
    , m_chatPollTimer(new QTimer(this))
{
    m_chatPollTimer->setSingleShot(true);
    connect(m_chatPollTimer, &QTimer::timeout, this, &YouTubePlatform::onPollLiveChat);

    connect(m_pollTimer, &QTimer::timeout, this, &YouTubePlatform::onCheckStreamStatus);
    m_pollIntervalMs = 15000;
}

YouTubePlatform::~YouTubePlatform()
{
    doDisconnect();
}

void YouTubePlatform::doConnect()
{
    resolveLiveStreamAndConnect();
}

void YouTubePlatform::doDisconnect()
{
    m_chatPollTimer->stop();
    m_pollTimer->stop();
    m_videoId.clear();
    m_continuationToken.clear();
    m_innerTubeApiKey.clear();
    m_innerTubeClientVersion.clear();
    m_isPolling = false;
    m_liveChatModeSelected = false;
    m_seenMessageIds.clear();
    m_seenMessageOrder.clear();
}

void YouTubePlatform::startChatListener()
{
    if (!m_isPolling)
    {
        doConnect();
    }
}

void YouTubePlatform::stopChatListener()
{
    doDisconnect();
}

void YouTubePlatform::resolveLiveStreamAndConnect()
{
    QString target = m_channelName.trimmed();
    if (target.isEmpty())
    {
        setStatus(PlatformStatus::OFFLINE, "Canal o video de YouTube no especificado");
        return;
    }

    QString liveUrl;
    if (target.startsWith("http://") || target.startsWith("https://"))
    {
        liveUrl = target;
    }
    else if (target.startsWith("@"))
    {
        liveUrl = QString("https://www.youtube.com/%1/live").arg(target);
    }
    else if (target.startsWith("UC") && target.length() == 24)
    {
        liveUrl = QString("https://www.youtube.com/channel/%1/live").arg(target);
    }
    else if (target.length() == 11 && !target.contains(' '))
    {
        m_videoId = target;
        liveUrl = QString("https://www.youtube.com/watch?v=%1").arg(target);
    }
    else
    {
        liveUrl = QString("https://www.youtube.com/@%1/live").arg(target);
    }

    Logger::instance().info("YouTube", QString("Resolviendo directo de YouTube: %1").arg(liveUrl));
    setStatus(PlatformStatus::CONNECTING);

    QMap<QString, QString> headers;
    headers["Accept-Language"] = "en-US,en;q=0.9";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";
    
    HttpHelper::instance().get(liveUrl, headers, [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        Q_UNUSED(statusCode);
        if (!success)
        {
            Logger::instance().error("YouTube", QString("Error al cargar directo de YouTube: %1").arg(err));
            setStatus(PlatformStatus::OFFLINE, "Canal no disponible o stream offline");
            return;
        }

        QString html = QString::fromUtf8(data);
        extractFromHtml(html);
    });
}

void YouTubePlatform::extractFromHtml(const QString& html)
{
    // Extract INNERTUBE_API_KEY
    QRegularExpression keyRegex("\"INNERTUBE_API_KEY\":\"([^\"]+)\"");
    auto keyMatch = keyRegex.match(html);
    if (keyMatch.hasMatch())
    {
        m_innerTubeApiKey = keyMatch.captured(1);
    }

    QRegularExpression versionRegex("\"INNERTUBE_CLIENT_VERSION\":\"([^\"]+)\"");
    const auto versionMatch = versionRegex.match(html);
    if (versionMatch.hasMatch())
    {
        m_innerTubeClientVersion = versionMatch.captured(1);
    }

    // Extract videoId if not present
    if (m_videoId.isEmpty())
    {
        QRegularExpression vidRegex("\"videoId\":\"([a-zA-Z0-9_-]{11})\"");
        auto vidMatch = vidRegex.match(html);
        if (vidMatch.hasMatch())
        {
            m_videoId = vidMatch.captured(1);
        }
    }

    // Extract continuation token from ytInitialData
    QRegularExpression contRegex("\"liveChatRenderer\"[\\s\\S]{0,4000}?\"continuation\":\"([^\"]+)\"");
    auto contMatch = contRegex.match(html);
    if (!contMatch.hasMatch())
    {
        contRegex = QRegularExpression("\"continuation\":\"([^\"]+)\"");
        contMatch = contRegex.match(html);
    }
    if (contMatch.hasMatch())
    {
        m_continuationToken = contMatch.captured(1);
        m_continuationToken.replace("\\u003d", "=");
        m_continuationToken.replace("\\u0026", "&");
        Logger::instance().info("YouTube", QString("Token inicial de chat obtenido del HTML (len=%1)").arg(m_continuationToken.size()));
    }

    // Extract live viewer count
    QRegularExpression viewRegex("\"viewCount\":\\{\"runs\":\\[\\{\"text\":\"([0-9,]+)\"");
    auto viewMatch = viewRegex.match(html);
    if (!viewMatch.hasMatch())
    {
        viewRegex = QRegularExpression("\"originalViewCount\":\"([0-9]+)\"");
        viewMatch = viewRegex.match(html);
    }
    if (!viewMatch.hasMatch())
    {
        viewRegex = QRegularExpression("\"viewCountText\":\\{\"runs\":\\[\\{\"text\":\"([0-9,]+)\"");
        viewMatch = viewRegex.match(html);
    }
    if (viewMatch.hasMatch())
    {
        QString countStr = viewMatch.captured(1).remove(',');
        int viewers = countStr.toInt();
        if (viewers >= 0) setViewerCount(viewers);
    }

    if (!m_continuationToken.isEmpty())
    {
        Logger::instance().info("YouTube", QString("Chat en vivo detectado. Video ID: %1").arg(m_videoId));
        setStatus(PlatformStatus::ONLINE);
        m_isPolling = true;
        m_chatPollTimer->start(200);
        m_pollTimer->start(m_pollIntervalMs);
    }
    else
    {
        Logger::instance().warn("YouTube", "No se detectó chat en vivo activo para este canal o video.");
        setStatus(PlatformStatus::OFFLINE, "Stream offline");
        setViewerCount(-1);
    }
}

void YouTubePlatform::onPollLiveChat()
{
    if (m_continuationToken.isEmpty()) return;

    fetchLiveChatInnerTube(m_continuationToken);
}

void YouTubePlatform::fetchLiveChatInnerTube(const QString& continuation)
{
    QString apiKey = !m_extraKey.isEmpty() ? m_extraKey : m_innerTubeApiKey;
    if (apiKey.isEmpty())
    {
        Logger::instance().warn("YouTube", "No se pudo obtener la clave pública de la página de YouTube.");
        m_chatPollTimer->start(3000);
        return;
    }

    QString url = QString("https://www.youtube.com/youtubei/v1/live_chat/get_live_chat?key=%1").arg(apiKey);

    QJsonObject contextObj;
    QJsonObject clientObj;
    clientObj["clientName"] = "WEB";
    clientObj["clientVersion"] = m_innerTubeClientVersion.isEmpty() ? "2.20250701.00.00" : m_innerTubeClientVersion;
    clientObj["hl"] = "en";
    clientObj["gl"] = "US";
    contextObj["client"] = clientObj;

    QJsonObject reqObj;
    reqObj["context"] = contextObj;
    reqObj["continuation"] = continuation;

    QMap<QString, QString> headers;
    headers["Content-Type"] = "application/json";
    headers["Origin"] = "https://www.youtube.com";
    headers["X-YouTube-Client-Name"] = "1";
    headers["X-YouTube-Client-Version"] = clientObj["clientVersion"].toString();
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36";

    QJsonDocument reqDoc(reqObj);

    HttpHelper::instance().post(url, headers, reqDoc.toJson(), [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        if (!success)
        {
            Logger::instance().warn("YouTube", QString("El lote de chat fue rechazado (HTTP %1): %2. Body: %3")
                .arg(statusCode).arg(err).arg(QString::fromUtf8(data.left(200))));
            m_chatPollTimer->start(2000);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject())
        {
            m_chatPollTimer->start(1500);
            return;
        }

        QJsonObject root = doc.object();
        QJsonObject continuationContents = root["continuationContents"].toObject();
        QJsonObject liveChatContinuation = continuationContents["liveChatContinuation"].toObject();
        if (liveChatContinuation.isEmpty() && root.contains("liveChatContinuation"))
        {
            liveChatContinuation = root["liveChatContinuation"].toObject();
        }

        // Automatic switch to Live Chat (All Messages) if we are currently on Top Chat
        if (!m_liveChatModeSelected)
        {
            QJsonObject header = liveChatContinuation["header"].toObject();
            QJsonObject liveChatHeader = header["liveChatHeaderRenderer"].toObject();
            QJsonObject viewSelector = liveChatHeader["viewSelector"].toObject();
            QJsonObject sortFilter = viewSelector["sortFilterSubMenuRenderer"].toObject();
            QJsonArray subMenuItems = sortFilter["subMenuItems"].toArray();

            // subMenuItems[0] = Top chat, subMenuItems[1] = Live chat
            if (subMenuItems.size() >= 2)
            {
                QJsonObject liveItem = subMenuItems.at(1).toObject();
                QJsonObject reloadCont = liveItem["continuation"].toObject()["reloadContinuationData"].toObject();
                QString liveToken = reloadCont["continuation"].toString();
                if (!liveToken.isEmpty())
                {
                    m_continuationToken = liveToken;
                    m_liveChatModeSelected = true;
                    Logger::instance().info("YouTube", "Conmutando de 'Top Chat' a 'Live Chat' (todos los mensajes) exitosamente.");
                    m_chatPollTimer->start(150);
                    return;
                }
            }
        }

        // Parse chat actions
        QJsonArray actions = liveChatContinuation["actions"].toArray();
        if (actions.isEmpty() && root.contains("actions"))
        {
            actions = root["actions"].toArray();
        }
        parseChatActions(actions);

        // Get next continuation token
        QJsonArray continuations = liveChatContinuation["continuations"].toArray();
        if (continuations.isEmpty() && root.contains("continuations"))
        {
            continuations = root["continuations"].toArray();
        }

        bool updated = false;
        for (const auto& contVal : continuations)
        {
            QJsonObject contObj = contVal.toObject();
            QJsonObject timedCont = contObj["timedContinuationData"].toObject();
            if (timedCont.isEmpty()) timedCont = contObj["invalidationContinuationData"].toObject();
            if (timedCont.isEmpty()) timedCont = contObj["reloadContinuationData"].toObject();
            if (timedCont.isEmpty()) timedCont = contObj["liveChatReplayContinuationData"].toObject();

            QString nextToken = timedCont["continuation"].toString();
            if (!nextToken.isEmpty())
            {
                m_continuationToken = nextToken;
                updated = true;
                int timeoutMs = timedCont["timeoutMs"].toInt(1500);
                m_chatPollIntervalMs = qBound(800, timeoutMs, 2500);
                break;
            }
        }

        if (!updated)
        {
            m_chatPollIntervalMs = 1500;
        }

        m_chatPollTimer->start(m_chatPollIntervalMs);
    });
}

bool YouTubePlatform::rememberMessageId(const QString& id)
{
    if (id.isEmpty() || m_seenMessageIds.contains(id)) return false;
    m_seenMessageIds.insert(id);
    m_seenMessageOrder.enqueue(id);
    while (m_seenMessageOrder.size() > 2000)
    {
        m_seenMessageIds.remove(m_seenMessageOrder.dequeue());
    }
    return true;
}

void YouTubePlatform::parseChatActions(const QJsonArray& actions)
{
    for (const auto& actVal : actions)
    {
        QJsonObject act = actVal.toObject();
        QJsonObject addChatItem = act["addChatItemAction"].toObject();
        if (addChatItem.isEmpty())
        {
            addChatItem = act["replayChatItemAction"].toObject();
        }
        if (addChatItem.isEmpty()) continue;

        QJsonObject item = addChatItem["item"].toObject();

        if (item.contains("liveChatTextMessageRenderer"))
        {
            parseTextMessageRenderer(item["liveChatTextMessageRenderer"].toObject());
        }
        else if (item.contains("liveChatPaidMessageRenderer"))
        {
            parsePaidMessageRenderer(item["liveChatPaidMessageRenderer"].toObject());
        }
        else if (item.contains("liveChatPaidStickerRenderer"))
        {
            parsePaidStickerRenderer(item["liveChatPaidStickerRenderer"].toObject());
        }
        else if (item.contains("liveChatMembershipItemRenderer"))
        {
            parseMembershipRenderer(item["liveChatMembershipItemRenderer"].toObject());
        }
        else if (item.contains("liveChatSponsorshipsGiftPurchaseAnnouncementRenderer"))
        {
            parseSponsorshipsGiftPurchase(item["liveChatSponsorshipsGiftPurchaseAnnouncementRenderer"].toObject());
        }
        else if (item.contains("liveChatSponsorshipsGiftRedemptionAnnouncementRenderer"))
        {
            parseSponsorshipsGiftRedemption(item["liveChatSponsorshipsGiftRedemptionAnnouncementRenderer"].toObject());
        }
        else if (item.contains("liveChatViewerEngagementMessageRenderer"))
        {
            QJsonObject eng = item["liveChatViewerEngagementMessageRenderer"].toObject();
            QString msgText = extractRunsText(eng["message"].toObject());
            if (!msgText.isEmpty())
            {
                QString msgId = eng["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
                if (rememberMessageId(msgId))
                {
                    ChatMessage msg;
                    msg.id = msgId;
                    msg.platform = "YouTube";
                    msg.displayName = "YouTube";
                    msg.username = "YouTube";
                    msg.message = msgText;
                    msg.timestamp = QDateTime::currentDateTime();
                    msg.userColor = "#9CA3AF";
                    msg.type = MessageType::MODERATION;
                    emitChatMessage(msg);
                }
            }
        }
    }
}

void YouTubePlatform::parseTextMessageRenderer(const QJsonObject& renderer)
{
    QString msgId = renderer["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "YouTube";
    msg.displayName = extractAuthorName(renderer);
    if (msg.displayName.isEmpty()) msg.displayName = "YouTube User";
    msg.username = msg.displayName;
    msg.userColor = "#EF4444";

    // Message text & emotes
    QJsonObject messageObj = renderer["message"].toObject();
    QJsonArray runs = messageObj["runs"].toArray();
    QString text;
    for (const auto& rVal : runs)
    {
        QJsonObject rObj = rVal.toObject();
        if (rObj.contains("text"))
        {
            text += rObj["text"].toString();
        }
        else if (rObj.contains("emoji"))
        {
            QJsonObject emoji = rObj["emoji"].toObject();
            const QJsonArray shortcuts = emoji["shortcuts"].toArray();
            const QString shortcut = shortcuts.isEmpty() ? emoji["emojiId"].toString() : shortcuts.at(0).toString();
            const QJsonArray thumbnails = emoji["image"].toObject()["thumbnails"].toArray();
            const QString imageUrl = thumbnails.isEmpty() ? QString() : thumbnails.last().toObject()["url"].toString();
            const int start = text.size();
            text += shortcut;
            if (!imageUrl.isEmpty() && !shortcut.isEmpty())
            {
                msg.emotes.append({ start, static_cast<int>(shortcut.size()), imageUrl, shortcut });
            }
        }
    }

    if (text.isEmpty() && messageObj.contains("simpleText"))
    {
        text = messageObj["simpleText"].toString();
    }
    if (text.isEmpty() && renderer.contains("messageText"))
    {
        text = renderer["messageText"].toString();
    }

    msg.message = text;
    msg.timestamp = QDateTime::currentDateTime();

    // Badges
    QJsonArray authorBadges = renderer["authorBadges"].toArray();
    for (const auto& bVal : authorBadges)
    {
        QJsonObject bObj = bVal.toObject();
        QJsonObject liveBadge = bObj["liveChatAuthorBadgeRenderer"].toObject();
        msg.badges.append(liveBadge["tooltip"].toString());
    }

    msg.type = MessageType::NORMAL;
    emitChatMessage(msg);
}

void YouTubePlatform::parsePaidMessageRenderer(const QJsonObject& renderer)
{
    QString msgId = renderer["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "YouTube";
    msg.displayName = extractAuthorName(renderer);
    if (msg.displayName.isEmpty()) msg.displayName = "YouTube User";
    msg.username = msg.displayName;
    msg.userColor = "#EF4444";

    // Purchase Amount Text (e.g. "$10.00", "€5.00")
    QString purchaseAmount = extractRunsText(renderer["purchaseAmountText"].toObject());
    QString text = extractRunsText(renderer["message"].toObject());

    msg.message = text;
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = MessageType::DONATION;
    msg.isDonation = true;

    // Parse currency and amount
    QRegularExpression amtRegex("([0-9.,]+)");
    auto m = amtRegex.match(purchaseAmount);
    if (m.hasMatch())
    {
        QString numStr = m.captured(1).replace(',', '.');
        msg.donationAmount = numStr.toDouble();
    }
    msg.currency = purchaseAmount.isEmpty() ? "$" : purchaseAmount;

    Donation don;
    don.id = msg.id;
    don.platform = "YouTube";
    don.username = msg.username;
    don.displayName = msg.displayName;
    don.amount = msg.donationAmount;
    don.currency = msg.currency;
    don.type = "Super Chat";
    don.message = text;
    don.timestamp = msg.timestamp;

    emitDonation(don);
    emitChatMessage(msg);
}

void YouTubePlatform::parsePaidStickerRenderer(const QJsonObject& renderer)
{
    QString msgId = renderer["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "YouTube";
    msg.displayName = extractAuthorName(renderer);
    if (msg.displayName.isEmpty()) msg.displayName = "YouTube User";
    msg.username = msg.displayName;
    msg.userColor = "#EF4444";

    QString purchaseAmount = extractRunsText(renderer["purchaseAmountText"].toObject());

    msg.message = QString("¡Ha enviado un Super Sticker! (%1)").arg(purchaseAmount);
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = MessageType::DONATION;
    msg.isDonation = true;
    msg.donationAmount = 2.00;
    msg.currency = purchaseAmount.isEmpty() ? "$" : purchaseAmount;

    Donation don;
    don.id = msg.id;
    don.platform = "YouTube";
    don.username = msg.username;
    don.displayName = msg.displayName;
    don.amount = 2.00;
    don.currency = msg.currency;
    don.type = "Super Sticker";
    don.message = msg.message;
    don.timestamp = msg.timestamp;

    emitDonation(don);
    emitChatMessage(msg);
}

void YouTubePlatform::parseMembershipRenderer(const QJsonObject& renderer)
{
    QString msgId = renderer["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "YouTube";
    msg.displayName = extractAuthorName(renderer);
    if (msg.displayName.isEmpty()) msg.displayName = "YouTube User";
    msg.username = msg.displayName;
    msg.userColor = "#EF4444";

    QString headerSubtext = extractRunsText(renderer["headerSubtext"].toObject());

    msg.message = headerSubtext.isEmpty() ? "¡Se ha unido como miembro del canal!" : headerSubtext;
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = MessageType::SUBSCRIPTION;
    msg.isDonation = true;
    msg.donationAmount = 4.99;
    msg.currency = "$";

    Donation don;
    don.id = msg.id;
    don.platform = "YouTube";
    don.username = msg.username;
    don.displayName = msg.displayName;
    don.amount = 4.99;
    don.currency = "$";
    don.type = "YouTube Membership";
    don.message = msg.message;
    don.timestamp = msg.timestamp;

    emitDonation(don);
    emitChatMessage(msg);
}

void YouTubePlatform::parseSponsorshipsGiftPurchase(const QJsonObject& renderer)
{
    QString msgId = renderer["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "YouTube";
    msg.displayName = extractAuthorName(renderer["header"].toObject()["liveChatSponsorshipsHeaderRenderer"].toObject());
    if (msg.displayName.isEmpty()) msg.displayName = extractAuthorName(renderer);
    if (msg.displayName.isEmpty()) msg.displayName = "YouTube User";
    msg.username = msg.displayName;
    msg.userColor = "#00B06F";

    QString primaryText = extractRunsText(renderer["header"].toObject()["liveChatSponsorshipsHeaderRenderer"].toObject()["primaryText"].toObject());
    if (primaryText.isEmpty()) primaryText = "¡Ha regalado membresías del canal!";

    msg.message = primaryText;
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = MessageType::GIFT;
    msg.isDonation = true;
    msg.donationAmount = 4.99;
    msg.currency = "$";

    Donation don;
    don.id = msg.id;
    don.platform = "YouTube";
    don.username = msg.username;
    don.displayName = msg.displayName;
    don.amount = 4.99;
    don.currency = "$";
    don.type = "YouTube Gift Memberships";
    don.message = primaryText;
    don.timestamp = msg.timestamp;

    emitDonation(don);
    emitChatMessage(msg);
}

void YouTubePlatform::parseSponsorshipsGiftRedemption(const QJsonObject& renderer)
{
    QString msgId = renderer["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "YouTube";
    msg.displayName = extractAuthorName(renderer);
    if (msg.displayName.isEmpty()) msg.displayName = "YouTube User";
    msg.username = msg.displayName;
    msg.userColor = "#00B06F";

    QString messageText = extractRunsText(renderer["message"].toObject());
    if (messageText.isEmpty()) messageText = "¡Ha recibido una membresía regalada!";

    msg.message = messageText;
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = MessageType::SUBSCRIPTION;

    emitChatMessage(msg);
}

void YouTubePlatform::onCheckStreamStatus()
{
    if (m_videoId.isEmpty()) return;

    QString url = QString("https://www.youtube.com/watch?v=%1").arg(m_videoId);
    QMap<QString, QString> headers;
    headers["Accept-Language"] = "en-US,en;q=0.9";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

    HttpHelper::instance().get(url, headers, [this](bool success, int statusCode, const QByteArray& data, const QString& err) {
        Q_UNUSED(statusCode);
        Q_UNUSED(err);
        if (!success) return;

        QString html = QString::fromUtf8(data);
        QRegularExpression viewRegex("\"viewCount\":\\{\"runs\":\\[\\{\"text\":\"([0-9,]+)\"");
        auto match = viewRegex.match(html);
        if (!match.hasMatch())
        {
            viewRegex = QRegularExpression("\"originalViewCount\":\"([0-9]+)\"");
            match = viewRegex.match(html);
        }
        if (match.hasMatch())
        {
            QString countStr = match.captured(1).remove(',');
            int viewers = countStr.toInt();
            if (viewers >= 0)
            {
                setViewerCount(viewers);
                setStatus(PlatformStatus::ONLINE);
            }
        }
    });
}
