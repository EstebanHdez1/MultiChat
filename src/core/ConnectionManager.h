#pragma once

#include <QObject>
#include <QMap>
#include <memory>
#include "IStreamingPlatform.h"
#include "TwitchPlatform.h"
#include "KickPlatform.h"
#include "YouTubePlatform.h"
#include "TikTokPlatform.h"
#include "ChannelConfig.h"

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    static ConnectionManager& instance();

    void initialize();
    void connectAll();
    void disconnectAll();

    void connectPlatform(const QString& platformName);
    void disconnectPlatform(const QString& platformName);

    BasePlatform* getPlatform(const QString& platformName) const;
    QList<BasePlatform*> getAllPlatforms() const;

    void updateChannelConfig(const ChannelConfig& config);

signals:
    void platformStateUpdated(const QString& platformName, PlatformStatus status);

private slots:
    void onChannelsChanged();

private:
    ConnectionManager(QObject* parent = nullptr);
    ~ConnectionManager() override = default;

    QMap<QString, BasePlatform*> m_platforms;
    TwitchPlatform* m_twitch;
    KickPlatform* m_kick;
    YouTubePlatform* m_youtube;
    TikTokPlatform* m_tiktok;
};
