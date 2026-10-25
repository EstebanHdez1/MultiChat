#pragma once

#include "BasePlatform.h"
#include "WebSocketWrapper.h"
#include <QRegularExpression>

class TwitchPlatform : public BasePlatform
{
    Q_OBJECT

public:
    explicit TwitchPlatform(QObject* parent = nullptr);
    ~TwitchPlatform() override;

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
    void parseIrcLine(const QString& line);
    void parsePrivMsg(const QMap<QString, QString>& tags, const QString& prefix, const QString& channel, const QString& text);
    void parseUserNotice(const QMap<QString, QString>& tags, const QString& channel, const QString& sysMsg, const QString& userMsg);
    QMap<QString, QString> parseTags(const QString& rawTags);

    WebSocketWrapper* m_ws;
    QString m_currentChannelJoined;
};
