#pragma once

#include <QObject>
#include <QMap>
#include "ChatMessage.h"
#include "Donation.h"
#include "PlatformStatus.h"

class EventManager : public QObject
{
    Q_OBJECT

public:
    static EventManager& instance();

    void postChatMessage(const ChatMessage& msg);
    void postDonation(const Donation& donation);
    void postStatusChange(const QString& platform, PlatformStatus status, const QString& detail = QString());
    void postViewerCount(const QString& platform, int count);

    PlatformStatus getPlatformStatus(const QString& platform) const;
    int getViewerCount(const QString& platform) const;
    int getTotalViewerCount() const;
    QMap<QString, int> getAllViewerCounts() const;

signals:
    void chatMessageReceived(const ChatMessage& msg);
    void donationReceived(const Donation& donation);
    void platformStatusChanged(const QString& platform, PlatformStatus status, const QString& detail);
    void viewerCountUpdated(const QString& platform, int count, int totalViewers);

private:
    EventManager(QObject* parent = nullptr);
    ~EventManager() override = default;

    QMap<QString, PlatformStatus> m_platformStatuses;
    QMap<QString, int> m_viewerCounts;
};
