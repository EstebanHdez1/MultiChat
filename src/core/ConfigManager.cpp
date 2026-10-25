#include "ConfigManager.h"
#include "Logger.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager s_instance;
    return s_instance;
}

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    // Default channels
    m_channels.append({ "Twitch", "", true, "" });
    m_channels.append({ "YouTube", "", true, "" });
    m_channels.append({ "Kick", "", true, "" });
    m_channels.append({ "TikTok", "", true, "" });
}

QString ConfigManager::getConfigFilePath() const
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appData);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
    return dir.filePath("multistream_chat_config.json");
}

void ConfigManager::loadConfig()
{
    QString path = getConfigFilePath();
    QFile file(path);
    if (!file.exists())
    {
        Logger::instance().info("ConfigManager", "No config file found, using defaults: " + path);
        return;
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        Logger::instance().error("ConfigManager", "Failed to open config file for reading: " + path);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        Logger::instance().warn("ConfigManager", "Config file corrupted or invalid JSON format");
        return;
    }

    QJsonObject root = doc.object();

    // Load Channels
    if (root.contains("channels") && root["channels"].isArray())
    {
        m_channels.clear();
        QJsonArray arr = root["channels"].toArray();
        for (const auto& val : arr)
        {
            QJsonObject obj = val.toObject();
            ChannelConfig cfg;
            cfg.platform = obj["platform"].toString();
            cfg.channelName = obj["channelName"].toString();
            cfg.enabled = obj["enabled"].toBool(true);
            cfg.extraKey = obj["extraKey"].toString();
            m_channels.append(cfg);
        }
    }

    // Load Style Config
    if (root.contains("style") && root["style"].isObject())
    {
        QJsonObject s = root["style"].toObject();
        m_styleConfig.themePreset = static_cast<ThemePreset>(s["themePreset"].toInt(0));
        m_styleConfig.fontFamily = s["fontFamily"].toString(m_styleConfig.fontFamily);
        m_styleConfig.fontSize = s["fontSize"].toInt(m_styleConfig.fontSize);
        m_styleConfig.showTimestamps = s["showTimestamps"].toBool(m_styleConfig.showTimestamps);
        m_styleConfig.showPlatformBadges = s["showPlatformBadges"].toBool(m_styleConfig.showPlatformBadges);
        m_styleConfig.showUserAvatars = s["showUserAvatars"].toBool(m_styleConfig.showUserAvatars);
        m_styleConfig.showUsernames = s["showUsernames"].toBool(m_styleConfig.showUsernames);
        m_styleConfig.showDonationHighlights = s["showDonationHighlights"].toBool(m_styleConfig.showDonationHighlights);
        m_styleConfig.showSubscriptionHighlights = s["showSubscriptionHighlights"].toBool(m_styleConfig.showSubscriptionHighlights);
        m_styleConfig.autoScroll = s["autoScroll"].toBool(m_styleConfig.autoScroll);
        m_styleConfig.maxVisibleMessages = s["maxVisibleMessages"].toInt(m_styleConfig.maxVisibleMessages);
        m_styleConfig.soundNotifications = s["soundNotifications"].toBool(m_styleConfig.soundNotifications);
        m_styleConfig.backgroundColor = s["backgroundColor"].toString(m_styleConfig.backgroundColor);
        m_styleConfig.cardBackgroundColor = s["cardBackgroundColor"].toString(m_styleConfig.cardBackgroundColor);
        m_styleConfig.textColor = s["textColor"].toString(m_styleConfig.textColor);
        m_styleConfig.usernameColor = s["usernameColor"].toString(m_styleConfig.usernameColor);
        m_styleConfig.timestampColor = s["timestampColor"].toString(m_styleConfig.timestampColor);
        m_styleConfig.backgroundOpacity = s["backgroundOpacity"].toDouble(m_styleConfig.backgroundOpacity);
        m_styleConfig.borderRadius = s["borderRadius"].toInt(m_styleConfig.borderRadius);
        m_styleConfig.transientDurationSec = s["transientDurationSec"].toDouble(m_styleConfig.transientDurationSec);
        m_styleConfig.showLikeCount = s["showLikeCount"].toBool(m_styleConfig.showLikeCount);
        m_styleConfig.likeCooldownSec = s["likeCooldownSec"].toInt(m_styleConfig.likeCooldownSec);
        m_styleConfig.showJoinEvents = s["showJoinEvents"].toBool(m_styleConfig.showJoinEvents);
        m_styleConfig.showShareEvents = s["showShareEvents"].toBool(m_styleConfig.showShareEvents);
        m_styleConfig.showFollowEvents = s["showFollowEvents"].toBool(m_styleConfig.showFollowEvents);
        m_styleConfig.showLikeEvents = s["showLikeEvents"].toBool(m_styleConfig.showLikeEvents);
    }

    Logger::instance().info("ConfigManager", "Configuration loaded successfully from " + path);
}

void ConfigManager::saveConfig()
{
    QString path = getConfigFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        Logger::instance().error("ConfigManager", "Failed to open config file for writing: " + path);
        return;
    }

    QJsonObject root;

    // Save channels
    QJsonArray arr;
    for (const auto& cfg : m_channels)
    {
        QJsonObject obj;
        obj["platform"] = cfg.platform;
        obj["channelName"] = cfg.channelName;
        obj["enabled"] = cfg.enabled;
        obj["extraKey"] = cfg.extraKey;
        arr.append(obj);
    }
    root["channels"] = arr;

    // Save style
    QJsonObject s;
    s["themePreset"] = static_cast<int>(m_styleConfig.themePreset);
    s["fontFamily"] = m_styleConfig.fontFamily;
    s["fontSize"] = m_styleConfig.fontSize;
    s["showTimestamps"] = m_styleConfig.showTimestamps;
    s["showPlatformBadges"] = m_styleConfig.showPlatformBadges;
    s["showUserAvatars"] = m_styleConfig.showUserAvatars;
    s["showUsernames"] = m_styleConfig.showUsernames;
    s["showDonationHighlights"] = m_styleConfig.showDonationHighlights;
    s["showSubscriptionHighlights"] = m_styleConfig.showSubscriptionHighlights;
    s["autoScroll"] = m_styleConfig.autoScroll;
    s["maxVisibleMessages"] = m_styleConfig.maxVisibleMessages;
    s["soundNotifications"] = m_styleConfig.soundNotifications;
    s["backgroundColor"] = m_styleConfig.backgroundColor;
    s["cardBackgroundColor"] = m_styleConfig.cardBackgroundColor;
    s["textColor"] = m_styleConfig.textColor;
    s["usernameColor"] = m_styleConfig.usernameColor;
    s["timestampColor"] = m_styleConfig.timestampColor;
    s["backgroundOpacity"] = m_styleConfig.backgroundOpacity;
    s["messageSpacing"] = m_styleConfig.messageSpacing;
    s["borderRadius"] = m_styleConfig.borderRadius;
    s["transientDurationSec"] = m_styleConfig.transientDurationSec;
    s["showLikeCount"] = m_styleConfig.showLikeCount;
    s["likeCooldownSec"] = m_styleConfig.likeCooldownSec;
    s["showJoinEvents"] = m_styleConfig.showJoinEvents;
    s["showShareEvents"] = m_styleConfig.showShareEvents;
    s["showFollowEvents"] = m_styleConfig.showFollowEvents;
    s["showLikeEvents"] = m_styleConfig.showLikeEvents;

    root["style"] = s;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    Logger::instance().info("ConfigManager", "Configuration saved to " + path);
}

QList<ChannelConfig> ConfigManager::getChannels() const
{
    return m_channels;
}

void ConfigManager::setChannels(const QList<ChannelConfig>& channels)
{
    m_channels = channels;
    saveConfig();
    emit channelsChanged();
}

void ConfigManager::updateChannel(const ChannelConfig& channel)
{
    bool found = false;
    for (int i = 0; i < m_channels.size(); ++i)
    {
        if (m_channels[i].platform.compare(channel.platform, Qt::CaseInsensitive) == 0)
        {
            m_channels[i] = channel;
            found = true;
            break;
        }
    }
    if (!found)
    {
        m_channels.append(channel);
    }
    saveConfig();
    emit channelsChanged();
}

void ConfigManager::removeChannel(const QString& platform)
{
    for (int i = 0; i < m_channels.size(); ++i)
    {
        if (m_channels[i].platform.compare(platform, Qt::CaseInsensitive) == 0)
        {
            m_channels.removeAt(i);
            break;
        }
    }
    saveConfig();
    emit channelsChanged();
}

AppStyleConfig ConfigManager::getStyleConfig() const
{
    return m_styleConfig;
}

void ConfigManager::setStyleConfig(const AppStyleConfig& style)
{
    m_styleConfig = style;
    saveConfig();
    emit styleChanged(m_styleConfig);
}

void ConfigManager::applyPreset(ThemePreset preset)
{
    m_styleConfig.themePreset = preset;
    switch (preset)
    {
    case ThemePreset::DARK:
        m_styleConfig.backgroundColor = "#12141A";
        m_styleConfig.cardBackgroundColor = "#1A1D24";
        m_styleConfig.textColor = "#F3F4F6";
        m_styleConfig.usernameColor = "#60A5FA";
        m_styleConfig.timestampColor = "#9CA3AF";
        m_styleConfig.backgroundOpacity = 1.0;
        break;
    case ThemePreset::LIGHT:
        m_styleConfig.backgroundColor = "#F9FAFB";
        m_styleConfig.cardBackgroundColor = "#FFFFFF";
        m_styleConfig.textColor = "#111827";
        m_styleConfig.usernameColor = "#2563EB";
        m_styleConfig.timestampColor = "#6B7280";
        m_styleConfig.backgroundOpacity = 1.0;
        break;
    case ThemePreset::STREAMER:
        m_styleConfig.backgroundColor = "#0D0E15";
        m_styleConfig.cardBackgroundColor = "#181B26";
        m_styleConfig.textColor = "#FFFFFF";
        m_styleConfig.usernameColor = "#FBBF24";
        m_styleConfig.timestampColor = "#A78BFA";
        m_styleConfig.backgroundOpacity = 0.95;
        break;
    case ThemePreset::MINIMAL:
        m_styleConfig.backgroundColor = "#000000";
        m_styleConfig.cardBackgroundColor = "#111111";
        m_styleConfig.textColor = "#E5E7EB";
        m_styleConfig.usernameColor = "#9CA3AF";
        m_styleConfig.timestampColor = "#4B5563";
        m_styleConfig.backgroundOpacity = 1.0;
        break;
    case ThemePreset::TRANSPARENT:
        m_styleConfig.backgroundColor = "transparent";
        m_styleConfig.cardBackgroundColor = "rgba(18, 20, 26, 0.65)";
        m_styleConfig.textColor = "#FFFFFF";
        m_styleConfig.usernameColor = "#93C5FD";
        m_styleConfig.timestampColor = "#D1D5DB";
        m_styleConfig.backgroundOpacity = 0.65;
        break;
    case ThemePreset::CUSTOM:
        break;
    }

    saveConfig();
    emit styleChanged(m_styleConfig);
}
