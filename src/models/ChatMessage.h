#pragma once

#include <QString>
#include <QDateTime>
#include <QStringList>

enum class MessageType
{
    NORMAL,
    DONATION,
    SUBSCRIPTION,
    GIFT,
    JOIN,
    SHARE,
    FOLLOW,
    LIKE,
    MODERATION,
    SYSTEM
};

struct ChatEmote
{
    int start = 0;
    int length = 0;
    QString imageUrl;
    QString altText;
};

struct ChatMessage
{
    QString id;
    QString platform;          // "Twitch", "YouTube", "Kick", "TikTok"
    QString username;
    QString displayName;
    QString message;
    QDateTime timestamp;
    
    // Styling & Metadata
    QString userColor;         // e.g. "#9146FF", "#EF4444", "#53FC18", "#FE2C55"
    QString avatarUrl;
    QStringList badges;        // e.g. "moderator", "vip", "subscriber", "verified"
    QList<ChatEmote> emotes;   // Image emotes positioned in the original message text
    
    // Event specific
    MessageType type = MessageType::NORMAL;
    bool isDonation = false;
    bool isTransient = false;  // If true, displayed as a temporary fading banner/toast
    double donationAmount = 0.0;
    QString currency;          // e.g. "USD", "EUR", "Bits", "Coins", "Roses"
    QString giftName;
    int giftCount = 1;
    
    // Helper to format timestamp
    QString formattedTime(const QString& format = "HH:mm:ss") const
    {
        return timestamp.isValid() ? timestamp.toString(format) : QDateTime::currentDateTime().toString(format);
    }
};
