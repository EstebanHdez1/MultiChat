#pragma once

#include "BasePlatform.h"
#include <QTimer>
#include <QRegularExpression>

#include <QSet>
#include <QQueue>

class YouTubePlatform : public BasePlatform
{
    Q_OBJECT

public:
    explicit YouTubePlatform(QObject* parent = nullptr);
    ~YouTubePlatform() override;

    void startChatListener() override;
    void stopChatListener() override;

protected:
    void doConnect() override;
    void doDisconnect() override;

private slots:
    void onPollLiveChat();
    void onCheckStreamStatus();

private:
    void resolveLiveStreamAndConnect();
    void extractFromHtml(const QString& html);
    void fetchLiveChatInnerTube(const QString& continuation);
    void parseChatActions(const QJsonArray& actions);
    void parseTextMessageRenderer(const QJsonObject& renderer);
    void parsePaidMessageRenderer(const QJsonObject& renderer);
    void parsePaidStickerRenderer(const QJsonObject& renderer);
    void parseMembershipRenderer(const QJsonObject& renderer);
    void parseSponsorshipsGiftPurchase(const QJsonObject& renderer);
    void parseSponsorshipsGiftRedemption(const QJsonObject& renderer);
    bool rememberMessageId(const QString& id);

    QString m_videoId;
    QString m_continuationToken;
    QString m_innerTubeApiKey;
    QString m_innerTubeClientVersion;
    QTimer* m_chatPollTimer;
    int m_chatPollIntervalMs = 1500;
    bool m_isPolling = false;
    bool m_liveChatModeSelected = false;
    QSet<QString> m_seenMessageIds;
    QQueue<QString> m_seenMessageOrder;
};
