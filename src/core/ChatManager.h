#pragma once

#include <QObject>
#include <QList>
#include <QMutex>
#include <QSet>
#include "ChatMessage.h"

struct ChatFilter
{
    bool showTwitch = true;
    bool showYouTube = true;
    bool showKick = true;
    bool showTikTok = true;
    
    bool showNormal = true;
    bool showDonations = true;
    bool showSubscriptions = true;
    bool showGifts = true;
    bool showEvents = true;
    bool showModeration = true;

    bool isMessageAllowed(const ChatMessage& msg) const
    {
        // Platform check
        if (msg.platform.compare("Twitch", Qt::CaseInsensitive) == 0 && !showTwitch) return false;
        if (msg.platform.compare("YouTube", Qt::CaseInsensitive) == 0 && !showYouTube) return false;
        if (msg.platform.compare("Kick", Qt::CaseInsensitive) == 0 && !showKick) return false;
        if (msg.platform.compare("TikTok", Qt::CaseInsensitive) == 0 && !showTikTok) return false;

        // Type check
        switch (msg.type)
        {
        case MessageType::NORMAL:
            return showNormal;
        case MessageType::DONATION:
            return showDonations;
        case MessageType::SUBSCRIPTION:
            return showSubscriptions;
        case MessageType::GIFT:
            return showGifts;
        case MessageType::JOIN:
        case MessageType::SHARE:
        case MessageType::FOLLOW:
        case MessageType::LIKE:
            return showEvents;
        case MessageType::MODERATION:
            return showModeration;
        default:
            return true;
        }
    }
};

class ChatManager : public QObject
{
    Q_OBJECT

public:
    static ChatManager& instance();

    void addMessage(const ChatMessage& msg);
    QList<ChatMessage> getFilteredMessages() const;
    QList<ChatMessage> getAllMessages() const;

    void setFilter(const ChatFilter& filter);
    ChatFilter getFilter() const;

    void setMaxMessages(int max);
    int getMaxMessages() const;

    void clear();

signals:
    void messageReceived(const ChatMessage& msg, bool passesFilter);
    void chatCleared();
    void filterChanged(const ChatFilter& filter);

private:
    ChatManager(QObject* parent = nullptr);
    ~ChatManager() override = default;

    mutable QMutex m_mutex;
    QList<ChatMessage> m_messages;
    ChatFilter m_filter;
    int m_maxMessages = 500;
};
