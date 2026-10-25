#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include "ChannelConfig.h"
#include "AppStyleConfig.h"

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager& instance();

    void loadConfig();
    void saveConfig();

    QList<ChannelConfig> getChannels() const;
    void setChannels(const QList<ChannelConfig>& channels);
    void updateChannel(const ChannelConfig& channel);
    void removeChannel(const QString& platform);

    AppStyleConfig getStyleConfig() const;
    void setStyleConfig(const AppStyleConfig& style);

    void applyPreset(ThemePreset preset);

signals:
    void channelsChanged();
    void styleChanged(const AppStyleConfig& style);

private:
    ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override = default;

    QString getConfigFilePath() const;

    QList<ChannelConfig> m_channels;
    AppStyleConfig m_styleConfig;
};
