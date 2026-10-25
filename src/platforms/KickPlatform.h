#pragma once

#include "BasePlatform.h"
#include "WebSocketWrapper.h"

class KickPlatform : public BasePlatform
{
    Q_OBJECT

public:
    explicit KickPlatform(QObject* parent = nullptr);
    ~KickPlatform() override;

    void startChatListener() override;
    void stopChatListener() override;

protected:
    void doConnect() override;
    void doDisconnect() override;

private slots:
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessage(const QString& message);
    void onPollStreamStatus();

private:
    void resolveChannelAndConnect();
    void requestCentrifugoTokenAndConnect();
    void subscribeToCentrifugoChannels();
    void parseKickEvent(const QString& eventName, const QJsonObject& dataObj);
    void handleChatMessageEvent(const QJsonObject& dataObj);
    void handleSubscriptionEvent(const QJsonObject& dataObj, bool isGift);

    WebSocketWrapper* m_ws;
    qint64 m_chatroomId = 0;
    qint64 m_kickChannelId = 0;
    QString m_slug;
    QString m_centrifugoToken;
};
