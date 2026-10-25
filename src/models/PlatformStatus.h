#pragma once

#include <QString>
#include <QColor>

enum class PlatformStatus
{
    OFFLINE,
    CONNECTING,
    ONLINE,
    RECONNECTING,
    ERROR_STATE,
    UNAVAILABLE,
    AUTH_REQUIRED
};

inline QString platformStatusToString(PlatformStatus status)
{
    switch (status)
    {
    case PlatformStatus::ONLINE:
        return "ONLINE";
    case PlatformStatus::OFFLINE:
        return "OFFLINE";
    case PlatformStatus::CONNECTING:
        return "CONNECTING";
    case PlatformStatus::RECONNECTING:
        return "RECONNECTING";
    case PlatformStatus::ERROR_STATE:
        return "ERROR";
    case PlatformStatus::UNAVAILABLE:
        return "UNAVAILABLE";
    case PlatformStatus::AUTH_REQUIRED:
        return "AUTH REQUIRED";
    default:
        return "UNKNOWN";
    }
}

inline QString platformStatusColorHex(PlatformStatus status)
{
    switch (status)
    {
    case PlatformStatus::ONLINE:
        return "#10B981"; // Vibrant green
    case PlatformStatus::CONNECTING:
    case PlatformStatus::RECONNECTING:
        return "#F59E0B"; // Amber
    case PlatformStatus::OFFLINE:
        return "#6B7280"; // Gray
    case PlatformStatus::ERROR_STATE:
    case PlatformStatus::UNAVAILABLE:
        return "#EF4444"; // Red
    case PlatformStatus::AUTH_REQUIRED:
        return "#8B5CF6"; // Purple
    default:
        return "#9CA3AF";
    }
}
