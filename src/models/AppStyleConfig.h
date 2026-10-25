#pragma once

#include <QString>

enum class ThemePreset
{
    DARK,
    LIGHT,
    STREAMER,
    MINIMAL,
    TRANSPARENT,
    CUSTOM
};

struct AppStyleConfig
{
    ThemePreset themePreset = ThemePreset::DARK;
    
    // Typography
    QString fontFamily = "Segoe UI";
    int fontSize = 14;
    
    // Visibility toggles
    bool showTimestamps = true;
    bool showPlatformBadges = true;
    bool showUserAvatars = false;
    bool showUsernames = true;
    bool showDonationHighlights = true;
    bool showSubscriptionHighlights = true;
    
    // Chat behavior
    bool autoScroll = true;
    int maxVisibleMessages = 500;
    bool soundNotifications = false;
    
    // Transient live events & Anti-spam configuration
    double transientDurationSec = 4.5;  // Duración en segundos antes de desaparecer (1 a 15 s)
    bool showLikeCount = false;         // false = solo quién dio like (anti-spam), true = mostrar conteo
    int likeCooldownSec = 10;           // Segundos de cooldown por usuario para likes
    bool showJoinEvents = true;         // Notificaciones de personas que se unen
    bool showShareEvents = true;        // Notificaciones de directos compartidos
    bool showFollowEvents = true;       // Notificaciones de nuevos seguidores
    bool showLikeEvents = true;         // Notificaciones de likes

    // Custom Colors & Styling
    QString backgroundColor = "#12141A";
    QString cardBackgroundColor = "#1A1D24";
    QString textColor = "#F3F4F6";
    QString usernameColor = "#60A5FA";
    QString timestampColor = "#9CA3AF";
    double backgroundOpacity = 1.0;
    int messageSpacing = 6;
    int borderRadius = 8;
    
    // Platform Color Accents
    QString twitchColor = "#9146FF";
    QString youtubeColor = "#FF0000";
    QString kickColor = "#53FC18";
    QString tiktokColor = "#FE2C55";
};
