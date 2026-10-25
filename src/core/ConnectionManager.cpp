#include "ConnectionManager.h"
#include "ConfigManager.h"
#include "Logger.h"

ConnectionManager& ConnectionManager::instance()
{
    static ConnectionManager s_instance;
    return s_instance;
}

ConnectionManager::ConnectionManager(QObject* parent)
    : QObject(parent)
    , m_twitch(new TwitchPlatform(this))
    , m_kick(new KickPlatform(this))
    , m_youtube(new YouTubePlatform(this))
    , m_tiktok(new TikTokPlatform(this))
{
    m_platforms["Twitch"] = m_twitch;
    m_platforms["Kick"] = m_kick;
    m_platforms["YouTube"] = m_youtube;
    m_platforms["TikTok"] = m_tiktok;

    connect(&ConfigManager::instance(), &ConfigManager::channelsChanged, this, &ConnectionManager::onChannelsChanged);
}

void ConnectionManager::initialize()
{
    onChannelsChanged();
}

void ConnectionManager::onChannelsChanged()
{
    QList<ChannelConfig> channels = ConfigManager::instance().getChannels();
    for (const auto& cfg : channels)
    {
        updateChannelConfig(cfg);
    }
}

void ConnectionManager::updateChannelConfig(const ChannelConfig& config)
{
    BasePlatform* platform = getPlatform(config.platform);
    if (platform)
    {
        platform->setChannel(config.channelName.toStdString(), config.extraKey.toStdString());
        Logger::instance().info("ConnectionManager", QString("Updated config for %1: channel='%2'")
            .arg(config.platform, config.channelName));
    }
}

void ConnectionManager::connectAll()
{
    Logger::instance().info("ConnectionManager", "Connecting all enabled platforms...");
    QList<ChannelConfig> channels = ConfigManager::instance().getChannels();
    for (const auto& cfg : channels)
    {
        if (cfg.enabled && !cfg.channelName.trimmed().isEmpty())
        {
            connectPlatform(cfg.platform);
        }
    }
}

void ConnectionManager::disconnectAll()
{
    Logger::instance().info("ConnectionManager", "Disconnecting all platforms...");
    for (auto* platform : m_platforms.values())
    {
        platform->disconnectPlatform();
    }
}

void ConnectionManager::connectPlatform(const QString& platformName)
{
    BasePlatform* platform = getPlatform(platformName);
    if (platform)
    {
        platform->connectPlatform();
    }
}

void ConnectionManager::disconnectPlatform(const QString& platformName)
{
    BasePlatform* platform = getPlatform(platformName);
    if (platform)
    {
        platform->disconnectPlatform();
    }
}

BasePlatform* ConnectionManager::getPlatform(const QString& platformName) const
{
    for (auto it = m_platforms.begin(); it != m_platforms.end(); ++it)
    {
        if (it.key().compare(platformName, Qt::CaseInsensitive) == 0)
        {
            return it.value();
        }
    }
    return nullptr;
}

QList<BasePlatform*> ConnectionManager::getAllPlatforms() const
{
    return m_platforms.values();
}
