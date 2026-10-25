#include "BasePlatform.h"
#include "EventManager.h"
#include "Logger.h"

BasePlatform::BasePlatform(const QString& platformName, QObject* parent)
    : QObject(parent)
    , m_platformName(platformName)
    , m_pollTimer(new QTimer(this))
{
}

BasePlatform::~BasePlatform()
{
    m_pollTimer->stop();
}

bool BasePlatform::connectPlatform()
{
    if (m_channelName.trimmed().isEmpty())
    {
        setStatus(PlatformStatus::OFFLINE, "No channel specified");
        return false;
    }

    Logger::instance().info(m_platformName, QString("Initiating connection for channel: %1").arg(m_channelName));
    setStatus(PlatformStatus::CONNECTING);
    doConnect();
    return true;
}

void BasePlatform::disconnectPlatform()
{
    m_pollTimer->stop();
    doDisconnect();
    setStatus(PlatformStatus::OFFLINE, "Disconnected by user");
    setViewerCount(-1);
}

bool BasePlatform::isOnline() const
{
    return m_status == PlatformStatus::ONLINE;
}

PlatformStatus BasePlatform::getStatus() const
{
    return m_status;
}

int BasePlatform::getViewerCount() const
{
    return m_viewerCount;
}

std::string BasePlatform::getPlatformName() const
{
    return m_platformName.toStdString();
}

void BasePlatform::setChannel(const std::string& channelName, const std::string& extraKey)
{
    m_channelName = QString::fromStdString(channelName).trimmed();
    m_extraKey = QString::fromStdString(extraKey).trimmed();
}

std::string BasePlatform::getChannel() const
{
    return m_channelName.toStdString();
}

void BasePlatform::setStatus(PlatformStatus status, const QString& detail)
{
    if (m_status == status && m_statusDetail == detail)
    {
        return;
    }

    m_status = status;
    m_statusDetail = detail;
    emit statusChanged(status);
    EventManager::instance().postStatusChange(m_platformName, status, detail);
}

void BasePlatform::setViewerCount(int count)
{
    m_viewerCount = count;
    emit viewerCountChanged(count);
    EventManager::instance().postViewerCount(m_platformName, count);
}

void BasePlatform::emitChatMessage(const ChatMessage& msg)
{
    emit messageReceived(msg);
    EventManager::instance().postChatMessage(msg);
}

void BasePlatform::emitDonation(const Donation& donation)
{
    emit donationReceived(donation);
    EventManager::instance().postDonation(donation);
}
