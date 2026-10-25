#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QHash>
#include <QSet>
#include <QList>
#include <QImage>
#include <QNetworkAccessManager>
#include <QTimer>
#include "ChatMessage.h"
#include "AppStyleConfig.h"
#include "ChatManager.h"

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget* parent = nullptr);
    ~ChatWidget() override = default;

    void appendMessage(const ChatMessage& msg, bool passesFilter);
    void applyStyle(const AppStyleConfig& style);
    void refreshAllMessages();
    void showTransientEvent(const ChatMessage& msg);

private slots:
    void onFilterChanged();
    void onClearChatClicked();
    void onAutoScrollToggled(bool checked);
    void onSimulateMessageClicked();
    void onSimulateDonationClicked();
    void onSimulateEventClicked();

private:
    void setupUi();
    void appendHtmlFormatted(const ChatMessage& msg);
    QString formatMessageHtml(const ChatMessage& msg);
    QString getPlatformBadgeHtml(const QString& platform) const;
    QString formatMessageBodyHtml(const ChatMessage& msg);
    void requestEmoteImage(const QString& url);

    QTextBrowser* m_chatBrowser;
    QWidget* m_filterBar;
    QWidget* m_transientContainer;
    QVBoxLayout* m_transientLayout;
    
    // Platform filter buttons
    QCheckBox* m_chkTwitch;
    QCheckBox* m_chkYouTube;
    QCheckBox* m_chkKick;
    QCheckBox* m_chkTikTok;

    // Type filter buttons
    QCheckBox* m_chkNormal;
    QCheckBox* m_chkDonations;
    QCheckBox* m_chkSubs;
    QCheckBox* m_chkGifts;
    QCheckBox* m_chkEvents;

    QPushButton* m_btnAutoScroll;
    QPushButton* m_btnClear;
    QPushButton* m_btnOpenWeb;
    QPushButton* m_btnSimulate;
    QLabel* m_lblMessageCount;
    QNetworkAccessManager m_emoteNetwork;
    QHash<QString, QImage> m_emoteImages;
    QList<QString> m_emoteImageOrder;
    QSet<QString> m_pendingEmoteImages;
    QSet<QString> m_failedEmoteImages;
    QList<QString> m_failedEmoteOrder;
    QTimer m_emoteRefreshTimer;

    AppStyleConfig m_style;
    bool m_autoScroll = true;
    int m_renderedCount = 0;
};
