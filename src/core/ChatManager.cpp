#include "ChatManager.h"
#include "Logger.h"
#include <QUuid>

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
    ChatMessage msgCopy = msg;
    if (msgCopy.id.isEmpty())
    {
        msgCopy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    bool allowed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!msgCopy.isTransient)
        {
            // Deduplicate: reject if identical message ID or same user+text was received within 15 seconds
            for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
            {
                if (!msgCopy.id.isEmpty() && it->id == msgCopy.id)
                {
                    return;
                }
                if (it->platform == msgCopy.platform && it->username == msgCopy.username && it->message == msgCopy.message)
                {
                    if (qAbs(it->timestamp.msecsTo(msgCopy.timestamp)) < 15000)
                    {
                        return;
                    }
                }
            }

            m_messages.append(msgCopy);
            while (m_messages.size() > m_maxMessages)
            {
                m_messages.removeFirst();
            }
        }
        allowed = m_filter.isMessageAllowed(msgCopy);
    }

    emit messageReceived(msgCopy, allowed);
}

ChatMessage ChatManager::getMessageById(const QString& id) const
{
    if (id.isEmpty()) return {};
    QMutexLocker locker(&m_mutex);
    for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
    {
        if (it->id == id)
        {
            return *it;
        }
    }
    return {};
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
