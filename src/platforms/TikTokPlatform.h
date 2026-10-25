#pragma once

#include "BasePlatform.h"
#include "WebSocketWrapper.h"
#include <QTimer>
#include <QRegularExpression>
#include <QSet>
#include <QQueue>
#include <QHash>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QJsonObject>

class TikTokPlatform : public BasePlatform
{
    Q_OBJECT

public:
    explicit TikTokPlatform(QObject* parent = nullptr);
    ~TikTokPlatform() override;

    void startChatListener() override;
    void stopChatListener() override;

    void setBrowserVisible(bool visible);
    bool isBrowserVisible() const { return m_browserVisible; }

protected:
    void doConnect() override;
    void doDisconnect() override;

private slots:
    void onCdpConnected();
    void onCdpDisconnected();
    void onCdpTextMessage(const QString& message);
    void onDomScrapeTimeout();

private:
    QString findBrowserExecutable() const;
    void ensureBrowserRunning(std::function<void(bool success)> callback);
    void checkCdpAvailable(std::function<void(bool isUp)> callback);
    void attachToLiveTab();
    void sendCdpCommand(const QString& method, const QJsonObject& params = QJsonObject());
    void parseCdpMessage(const QJsonObject& root);
    void parseDomScrapeResult(const QJsonObject& result);

    void parseWebcastResponse(const QByteArray& data);
    void parseBinaryProtobuf(const QByteArray& data);
    void handleChatMessage(const QJsonObject& msgObj);
    void handleGiftMessage(const QJsonObject& msgObj);
    void handleMemberMessage(const QJsonObject& msgObj);
    void handleSocialMessage(const QJsonObject& msgObj);
    void handleLikeMessage(const QJsonObject& msgObj);
    bool rememberMessageId(const QString& id, const QString& user = QString(), const QString& text = QString());

    QString m_username;
    QString m_targetId;
    QString m_wsDebuggerUrl;
    int m_cdpPort = 9222;
    int m_cdpMsgId = 1;
    bool m_browserVisible = true;
    bool m_isConnecting = false;

    QProcess* m_browserProcess = nullptr;
    WebSocketWrapper* m_cdpSocket = nullptr;
    QNetworkAccessManager* m_netManager = nullptr;
    QTimer* m_domScrapeTimer = nullptr;

    QSet<QString> m_seenMessageIds;
    QQueue<QString> m_seenMessageOrder;
    QHash<QString, qint64> m_lastLikeByUser;
    QHash<QString, qint64> m_recentContentSigs;
    qint64 m_lastWsMessageTime = 0;
    bool m_hasReceivedWebSocketFrame = false;
};
