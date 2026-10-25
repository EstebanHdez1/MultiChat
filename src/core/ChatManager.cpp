#include "ChatManager.h"
#include "Logger.h"

ChatManager& ChatManager::instance()
{
    static ChatManager s_instance;
    return s_instance;
}

ChatManager::ChatManager(QObject* parent)
    : QObject(parent)
{
}

void ChatManager::addMessage(const ChatMessage& msg)
{
    bool allowed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!msg.isTransient)
        {
            // Deduplicate: reject if identical message ID or same user+text was received within 15 seconds
            for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
            {
                if (!msg.id.isEmpty() && it->id == msg.id)
                {
                    return;
                }
                if (it->platform == msg.platform && it->username == msg.username && it->message == msg.message)
                {
                    if (qAbs(it->timestamp.msecsTo(msg.timestamp)) < 15000)
                    {
                        return;
                    }
                }
            }

            m_messages.append(msg);
            while (m_messages.size() > m_maxMessages)
            {
                m_messages.removeFirst();
            }
        }
        allowed = m_filter.isMessageAllowed(msg);
    }

    emit messageReceived(msg, allowed);
}

QList<ChatMessage> ChatManager::getFilteredMessages() const
{
    QMutexLocker locker(&m_mutex);
    QList<ChatMessage> result;
    for (const auto& msg : m_messages)
    {
        if (m_filter.isMessageAllowed(msg))
        {
            result.append(msg);
        }
    }
    return result;
}

QList<ChatMessage> ChatManager::getAllMessages() const
{
    QMutexLocker locker(&m_mutex);
    return m_messages;
}

void ChatManager::setFilter(const ChatFilter& filter)
{
    {
        QMutexLocker locker(&m_mutex);
        m_filter = filter;
    }
    emit filterChanged(m_filter);
}

ChatFilter ChatManager::getFilter() const
{
    QMutexLocker locker(&m_mutex);
    return m_filter;
}

void ChatManager::setMaxMessages(int max)
{
    QMutexLocker locker(&m_mutex);
    m_maxMessages = qMax(50, max);
    while (m_messages.size() > m_maxMessages)
    {
        m_messages.removeFirst();
    }
}

int ChatManager::getMaxMessages() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxMessages;
}

void ChatManager::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_messages.clear();
    }
    emit chatCleared();
}
