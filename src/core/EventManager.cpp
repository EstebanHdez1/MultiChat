#include "EventManager.h"
#include "ChatManager.h"
#include "DonationManager.h"
#include "Logger.h"

EventManager& EventManager::instance()
{
    static EventManager s_instance;
    return s_instance;
}

EventManager::EventManager(QObject* parent)
    : QObject(parent)
{
    m_platformStatuses["Twitch"] = PlatformStatus::OFFLINE;
    m_platformStatuses["YouTube"] = PlatformStatus::OFFLINE;
    m_platformStatuses["Kick"] = PlatformStatus::OFFLINE;
    m_platformStatuses["TikTok"] = PlatformStatus::OFFLINE;

    m_viewerCounts["Twitch"] = -1;
    m_viewerCounts["YouTube"] = -1;
    m_viewerCounts["Kick"] = -1;
    m_viewerCounts["TikTok"] = -1;
}

void EventManager::postChatMessage(const ChatMessage& msg)
{
    ChatManager::instance().addMessage(msg);
    emit chatMessageReceived(msg);
}

void EventManager::postDonation(const Donation& donation)
{
    DonationManager::instance().addDonation(donation);
    emit donationReceived(donation);
}

void EventManager::postStatusChange(const QString& platform, PlatformStatus status, const QString& detail)
{
    m_platformStatuses[platform] = status;
    if (status == PlatformStatus::OFFLINE || status == PlatformStatus::ERROR_STATE || status == PlatformStatus::UNAVAILABLE)
    {
        m_viewerCounts[platform] = -1;
    }

    Logger::instance().info("EventManager", QString("Status [%1]: %2 %3")
        .arg(platform, platformStatusToString(status), detail.isEmpty() ? "" : QString("(%1)").arg(detail)));

    emit platformStatusChanged(platform, status, detail);
}

void EventManager::postViewerCount(const QString& platform, int count)
{
    m_viewerCounts[platform] = count;
    int total = getTotalViewerCount();
    emit viewerCountUpdated(platform, count, total);
}

PlatformStatus EventManager::getPlatformStatus(const QString& platform) const
{
    return m_platformStatuses.value(platform, PlatformStatus::OFFLINE);
}

int EventManager::getViewerCount(const QString& platform) const
{
    return m_viewerCounts.value(platform, -1);
}

int EventManager::getTotalViewerCount() const
{
    int total = 0;
    bool hasAny = false;
    for (auto it = m_viewerCounts.begin(); it != m_viewerCounts.end(); ++it)
    {
        if (it.value() >= 0)
        {
            total += it.value();
            hasAny = true;
        }
    }
    return hasAny ? total : -1;
}

QMap<QString, int> EventManager::getAllViewerCounts() const
{
    return m_viewerCounts;
}
