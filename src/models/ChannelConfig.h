#pragma once

#include <QString>

struct ChannelConfig
{
    QString platform;      // "Twitch", "YouTube", "Kick", "TikTok"
    QString channelName;   // e.g. "shroud", "@streamer", "streamer_id"
    bool enabled = true;
    QString extraKey;      // Optional API key or room ID if required
};
