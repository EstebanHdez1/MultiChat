#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include "IStreamingPlatform.h"
#include "ChatMessage.h"
#include "Donation.h"
#include "PlatformStatus.h"

class BasePlatform : public QObject, public IStreamingPlatform
{
    Q_OBJECT

public:
    explicit BasePlatform(const QString& platformName, QObject* parent = nullptr);
    ~BasePlatform() override;

    // IStreamingPlatform implementation
    bool connectPlatform() override;
    void disconnectPlatform() override;

    bool isOnline() const override;
    PlatformStatus getStatus() const override;

    int getViewerCount() const override;
    std::string getPlatformName() const override;

    void setChannel(const std::string& channelName, const std::string& extraKey = "") override;
    std::string getChannel() const override;

signals:
    void statusChanged(PlatformStatus status);
    void messageReceived(const ChatMessage& msg);
    void donationReceived(const Donation& donation);
    void viewerCountChanged(int count);

protected:
    void setStatus(PlatformStatus status, const QString& detail = QString());
    void setViewerCount(int count);
    void emitChatMessage(const ChatMessage& msg);
    void emitDonation(const Donation& donation);

    virtual void doConnect() = 0;
    virtual void doDisconnect() = 0;

    QString m_platformName;
    QString m_channelName;
    QString m_extraKey;
    PlatformStatus m_status = PlatformStatus::OFFLINE;
    // Keep the last detail as part of the state.  Polling platforms call
    // setStatus frequently; re-emitting an unchanged state floods the event
    // log and needlessly repaints the UI.
    QString m_statusDetail;
    int m_viewerCount = -1;

    // Periodic poll timer for checking stream status & viewers
    QTimer* m_pollTimer;
    int m_pollIntervalMs = 15000;
};
