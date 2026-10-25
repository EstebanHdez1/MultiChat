#include "ChatWidget.h"
#include "ChatManager.h"
#include "EventManager.h"
#include "ConfigManager.h"
#include <QScrollBar>
#include <QRandomGenerator>
#include <QIcon>
#include <QBuffer>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextDocument>
#include <QFrame>
#include <algorithm>

namespace
{
QString platformLogoDataUri(const QString& iconPath)
{
    QPixmap logo = QIcon(iconPath).pixmap(18, 18);
    if (logo.isNull())
    {
        return {};
    }

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    logo.save(&buffer, "PNG");
    return QString("data:image/png;base64,%1").arg(QString::fromLatin1(png.toBase64()));
}
}

ChatWidget::ChatWidget(QWidget* parent)
    : QWidget(parent)
    , m_chatBrowser(new QTextBrowser(this))
    , m_transientContainer(new QWidget(this))
    , m_transientLayout(new QVBoxLayout(m_transientContainer))
{
    m_emoteRefreshTimer.setSingleShot(true);
    m_emoteRefreshTimer.setInterval(120);
    connect(&m_emoteRefreshTimer, &QTimer::timeout, this, &ChatWidget::refreshAllMessages);
    setupUi();
    m_style = ConfigManager::instance().getStyleConfig();
    applyStyle(m_style);

    connect(&ChatManager::instance(), &ChatManager::messageReceived, this, &ChatWidget::appendMessage);
    connect(&ChatManager::instance(), &ChatManager::chatCleared, this, &ChatWidget::refreshAllMessages);
    connect(&ConfigManager::instance(), &ConfigManager::styleChanged, this, &ChatWidget::applyStyle);
}

void ChatWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);

    // Top Filter Bar
    m_filterBar = new QWidget(this);
    m_filterBar->setStyleSheet("background-color: #1A1D24; border-radius: 8px; padding: 4px;");
    auto* filterBar = m_filterBar;
    auto* filterLayout = new QHBoxLayout(m_filterBar);
    filterLayout->setContentsMargins(10, 6, 10, 6);
    filterLayout->setSpacing(10);

    auto* lblFilterIcon = new QLabel(filterBar);
    lblFilterIcon->setPixmap(QIcon(":/icons/filter.svg").pixmap(14, 14));
    auto* lblFilter = new QLabel("Filtros:", filterBar);
    lblFilter->setStyleSheet("font-weight: bold; color: #9CA3AF; font-size: 11px;");
    filterLayout->addWidget(lblFilterIcon);
    filterLayout->addWidget(lblFilter);

    // Platform filters
    auto createPlatformCheck = [this, filterBar](const QString& name, const QString& color, const QString& iconPath) -> QCheckBox* {
        auto* chk = new QCheckBox(filterBar);
        chk->setIcon(QIcon(iconPath));
        chk->setIconSize(QSize(16, 16));
        chk->setText(" ");
        chk->setMinimumWidth(38);
        chk->setToolTip(name);
        chk->setAccessibleName(name);
        chk->setChecked(true);
        chk->setCursor(Qt::PointingHandCursor);
        chk->setStyleSheet(QString(
            "QCheckBox { color: %1; font-weight: bold; font-size: 12px; spacing: 5px; }"
            "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px; border: 1px solid #4B5563; background: #111827; }"
            "QCheckBox::indicator:checked { background: %1; border-color: %1; }"
        ).arg(color));
        connect(chk, &QCheckBox::toggled, this, &ChatWidget::onFilterChanged);
        return chk;
    };

    m_chkTwitch = createPlatformCheck("Twitch", "#9146FF", ":/icons/twitch.svg");
    m_chkYouTube = createPlatformCheck("YouTube", "#EF4444", ":/icons/youtube.svg");
    m_chkKick = createPlatformCheck("Kick", "#53FC18", ":/icons/kick.svg");
    m_chkTikTok = createPlatformCheck("TikTok", "#FE2C55", ":/icons/tiktok.svg");

    filterLayout->addWidget(m_chkTwitch);
    filterLayout->addWidget(m_chkYouTube);
    filterLayout->addWidget(m_chkKick);
    filterLayout->addWidget(m_chkTikTok);

    auto* sep1 = new QFrame(filterBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #374151;");
    filterLayout->addWidget(sep1);

    // Type filters
    auto createTypeCheck = [this, filterBar](const QString& name) -> QCheckBox* {
        auto* chk = new QCheckBox(name, filterBar);
        chk->setChecked(true);
        chk->setCursor(Qt::PointingHandCursor);
        chk->setStyleSheet(
            "QCheckBox { color: #D1D5DB; font-size: 11px; spacing: 4px; }"
            "QCheckBox::indicator { width: 12px; height: 12px; border-radius: 2px; border: 1px solid #4B5563; background: #111827; }"
            "QCheckBox::indicator:checked { background: #3B82F6; border-color: #3B82F6; }"
        );
        connect(chk, &QCheckBox::toggled, this, &ChatWidget::onFilterChanged);
        return chk;
    };

    m_chkNormal = createTypeCheck("Normal");
    m_chkDonations = createTypeCheck("Donaciones");
    m_chkSubs = createTypeCheck("Subs");
    m_chkGifts = createTypeCheck("Gifts");
    m_chkEvents = createTypeCheck("Eventos Live");
    m_chkEvents->setToolTip("Notificaciones temporales de usuarios que se unen, comparten o dan like");

    filterLayout->addWidget(m_chkNormal);
    filterLayout->addWidget(m_chkDonations);
    filterLayout->addWidget(m_chkSubs);
    filterLayout->addWidget(m_chkGifts);
    filterLayout->addWidget(m_chkEvents);

    filterLayout->addStretch();

    // Tools in Filter Bar
    m_btnAutoScroll = new QPushButton("Auto-scroll ON", filterBar);
    m_btnAutoScroll->setCheckable(true);
    m_btnAutoScroll->setChecked(true);
    m_btnAutoScroll->setCursor(Qt::PointingHandCursor);
    m_btnAutoScroll->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 11px; font-weight: bold; }"
        "QPushButton:checked { background: #2563EB; }"
        "QPushButton:!checked { background: #4B5563; }"
    );
    connect(m_btnAutoScroll, &QPushButton::toggled, this, &ChatWidget::onAutoScrollToggled);
    filterLayout->addWidget(m_btnAutoScroll);

    m_btnClear = new QPushButton("Limpiar", filterBar);
    m_btnClear->setIcon(QIcon(":/icons/trash.svg"));
    m_btnClear->setIconSize(QSize(12, 12));
    m_btnClear->setCursor(Qt::PointingHandCursor);
    m_btnClear->setStyleSheet("QPushButton { background: #374151; color: #E5E7EB; border: none; border-radius: 4px; padding: 4px 8px; font-size: 11px; } QPushButton:hover { background: #4B5563; }");
    connect(m_btnClear, &QPushButton::clicked, this, &ChatWidget::onClearChatClicked);
    filterLayout->addWidget(m_btnClear);

    m_btnOpenWeb = new QPushButton("Abrir Web", filterBar);
    m_btnOpenWeb->setIcon(QIcon(":/icons/globe.svg"));
    m_btnOpenWeb->setIconSize(QSize(12, 12));
    m_btnOpenWeb->setCursor(Qt::PointingHandCursor);
    m_btnOpenWeb->setToolTip("Abre la interfaz web moderna en tu navegador o comparte con tu móvil");
    m_btnOpenWeb->setStyleSheet("QPushButton { background: #0D9488; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 11px; font-weight: bold; } QPushButton:hover { background: #0F766E; }");
    connect(m_btnOpenWeb, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("http://localhost:8080"));
    });
    filterLayout->addWidget(m_btnOpenWeb);

    m_btnSimulate = new QPushButton("Simular", filterBar);
    m_btnSimulate->setIcon(QIcon(":/icons/dice.svg"));
    m_btnSimulate->setIconSize(QSize(13, 13));
    m_btnSimulate->setToolTip("Simular mensajes, donaciones y eventos de directo");
    m_btnSimulate->setCursor(Qt::PointingHandCursor);
    m_btnSimulate->setStyleSheet(
        "QPushButton { background: #374151; color: #E5E7EB; border: none; border-radius: 4px; padding: 4px 8px; font-size: 11px; font-weight: 500; }"
        "QPushButton:hover { background: #4B5563; }"
    );
    auto* simMenu = new QMenu(m_btnSimulate);
    simMenu->setStyleSheet(
        "QMenu { background-color: #1F2937; color: #F3F4F6; border: 1px solid #374151; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 14px; border-radius: 4px; font-size: 11px; }"
        "QMenu::item:selected { background-color: #2563EB; }"
    );
    auto* actMsg = simMenu->addAction(QIcon(":/icons/chat.svg"), "Simular Mensaje");
    connect(actMsg, &QAction::triggered, this, &ChatWidget::onSimulateMessageClicked);
    auto* actDon = simMenu->addAction(QIcon(":/icons/gift.svg"), "Simular Donación");
    connect(actDon, &QAction::triggered, this, &ChatWidget::onSimulateDonationClicked);
    auto* actEvt = simMenu->addAction(QIcon(":/icons/bolt.svg"), "Simular Evento Live");
    connect(actEvt, &QAction::triggered, this, &ChatWidget::onSimulateEventClicked);
    m_btnSimulate->setMenu(simMenu);
    filterLayout->addWidget(m_btnSimulate);

    mainLayout->addWidget(m_filterBar);

    // Chat Browser
    m_chatBrowser->setOpenExternalLinks(true);
    m_chatBrowser->setReadOnly(true);
    m_chatBrowser->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #12141A;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  color: #F3F4F6;"
        "}"
    );
    mainLayout->addWidget(m_chatBrowser, 1);

    // Transient Notifications Container (Placed at bottom, below chat)
    m_transientContainer->setStyleSheet("background: transparent; border: none;");
    m_transientLayout->setContentsMargins(0, 0, 0, 0);
    m_transientLayout->setSpacing(4);
    m_transientContainer->hide();
    mainLayout->addWidget(m_transientContainer);
}

void ChatWidget::applyStyle(const AppStyleConfig& style)
{
    m_style = style;
    m_autoScroll = style.autoScroll;
    m_filterBar->setVisible(style.backgroundColor != "transparent");
    m_btnAutoScroll->setChecked(m_autoScroll);

    QString bg = style.backgroundColor;
    if (bg == "transparent")
    {
        bg = "rgba(0,0,0,0)";
    }

    m_chatBrowser->setStyleSheet(QString(
        "QTextBrowser {"
        "  background-color: %1;"
        "  border: 1px solid #1F2937;"
        "  border-radius: %2px;"
        "  padding: 10px;"
        "  color: %3;"
        "  font-family: '%4';"
        "  font-size: %5px;"
        "}"
    ).arg(bg).arg(style.borderRadius).arg(style.textColor, style.fontFamily).arg(style.fontSize));

    refreshAllMessages();
}

void ChatWidget::appendMessage(const ChatMessage& msg, bool passesFilter)
{
    if (!passesFilter) return;

    // Transient events (joins, shares, follows, likes) are shown as temporary toasts,
    // and NEVER stored permanently in the chat history log.
    if (msg.isTransient)
    {
        if (m_chkEvents->isChecked())
        {
            showTransientEvent(msg);
        }
        return;
    }

    appendHtmlFormatted(msg);
    m_renderedCount++;

    if (m_renderedCount > ChatManager::instance().getMaxMessages())
    {
        refreshAllMessages();
        return;
    }

    if (m_autoScroll)
    {
        m_chatBrowser->verticalScrollBar()->setValue(m_chatBrowser->verticalScrollBar()->maximum());
    }
}

void ChatWidget::showTransientEvent(const ChatMessage& msg)
{
    QString accentColor = "#FE2C55";
    QString actionIconPath = ":/icons/user.svg";

    switch (msg.type)
    {
    case MessageType::JOIN:
        accentColor = "#3B82F6";
        actionIconPath = ":/icons/user.svg";
        break;
    case MessageType::SHARE:
        accentColor = "#10B981";
        actionIconPath = ":/icons/share.svg";
        break;
    case MessageType::FOLLOW:
        accentColor = "#EC4899";
        actionIconPath = ":/icons/star.svg";
        break;
    case MessageType::LIKE:
        accentColor = "#EF4444";
        actionIconPath = ":/icons/heart.svg";
        break;
    default:
        accentColor = msg.userColor.isEmpty() ? "#FE2C55" : msg.userColor;
        actionIconPath = ":/icons/bolt.svg";
        break;
    }

    // Create toast frame
    auto* toast = new QFrame(m_transientContainer);
    toast->setStyleSheet(QString(
        "QFrame {"
        "  background-color: rgba(26, 29, 36, 0.95);"
        "  border: 1px solid #374151;"
        "  border-left: 4px solid %1;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "}"
    ).arg(accentColor));

    auto* toastLayout = new QHBoxLayout(toast);
    toastLayout->setContentsMargins(6, 3, 6, 3);
    toastLayout->setSpacing(8);

    // Platform logo
    QString iconPath = ":/icons/tiktok.svg";
    if (msg.platform.compare("Twitch", Qt::CaseInsensitive) == 0) iconPath = ":/icons/twitch.svg";
    else if (msg.platform.compare("YouTube", Qt::CaseInsensitive) == 0) iconPath = ":/icons/youtube.svg";
    else if (msg.platform.compare("Kick", Qt::CaseInsensitive) == 0) iconPath = ":/icons/kick.svg";

    auto* logoLabel = new QLabel(toast);
    logoLabel->setPixmap(QIcon(iconPath).pixmap(14, 14));
    toastLayout->addWidget(logoLabel);

    // Event action icon
    auto* actionIcon = new QLabel(toast);
    actionIcon->setPixmap(QIcon(actionIconPath).pixmap(14, 14));
    toastLayout->addWidget(actionIcon);

    // Text content
    auto* textLabel = new QLabel(toast);
    textLabel->setTextFormat(Qt::RichText);
    QString nameHtml = QString("<span style=\"color:%1; font-weight:bold;\">%2</span>")
                           .arg(msg.userColor.isEmpty() ? accentColor : msg.userColor, msg.displayName.toHtmlEscaped());
    QString msgHtml = QString("<span style=\"color:#E5E7EB; font-size:11px;\">%1 %2</span>")
                          .arg(nameHtml, msg.message.toHtmlEscaped());
    textLabel->setText(msgHtml);
    toastLayout->addWidget(textLabel, 1);

    // Timestamp
    auto* timeLabel = new QLabel(msg.formattedTime(), toast);
    timeLabel->setStyleSheet("color: #9CA3AF; font-size: 10px; font-family: monospace;");
    toastLayout->addWidget(timeLabel);

    // Max 3 visible at the same time
    while (m_transientLayout->count() >= 3)
    {
        QLayoutItem* oldItem = m_transientLayout->takeAt(0);
        if (oldItem && oldItem->widget())
        {
            oldItem->widget()->deleteLater();
        }
        delete oldItem;
    }

    m_transientLayout->addWidget(toast);
    m_transientContainer->show();

    // Auto-dismiss using configured duration
    int durationMs = qBound(1000, static_cast<int>((m_style.transientDurationSec > 0.0 ? m_style.transientDurationSec : 4.5) * 1000), 30000);
    QTimer::singleShot(durationMs, toast, [this, toast]() {
        toast->deleteLater();
        QTimer::singleShot(60, this, [this]() {
            if (m_transientLayout->count() == 0)
            {
                m_transientContainer->hide();
            }
        });
    });
}

void ChatWidget::appendHtmlFormatted(const ChatMessage& msg)
{
    QString html = formatMessageHtml(msg);
    m_chatBrowser->append(html);
}

QString ChatWidget::getPlatformBadgeHtml(const QString& platform) const
{
    QString bgColor;
    QString iconPath;

    if (platform.compare("Twitch", Qt::CaseInsensitive) == 0)
    {
        bgColor = m_style.twitchColor;
        iconPath = ":/icons/twitch.svg";
    }
    else if (platform.compare("YouTube", Qt::CaseInsensitive) == 0)
    {
        bgColor = m_style.youtubeColor;
        iconPath = ":/icons/youtube.svg";
    }
    else if (platform.compare("Kick", Qt::CaseInsensitive) == 0)
    {
        bgColor = m_style.kickColor;
        iconPath = ":/icons/kick.svg";
    }
    else if (platform.compare("TikTok", Qt::CaseInsensitive) == 0)
    {
        bgColor = m_style.tiktokColor;
        iconPath = ":/icons/tiktok.svg";
    }
    else
    {
        bgColor = "#6B7280";
        return QString("<span style=\"background-color: %1; color: #FFFFFF; font-size: 10px; font-weight: bold; padding: 2px 6px; border-radius: 4px; margin-right: 6px;\">%2</span>")
            .arg(bgColor, platform.toHtmlEscaped().toUpper());
    }

    const QString logo = platformLogoDataUri(iconPath);
    return QString("<span style=\"background-color: transparent; padding: 1px 3px; margin-right: 6px; vertical-align: middle;\"><img src=\"%1\" width=\"16\" height=\"16\" /></span>")
        .arg(logo);
}

void ChatWidget::requestEmoteImage(const QString& url)
{
    if (url.isEmpty() || m_emoteImages.contains(url) || m_pendingEmoteImages.contains(url)
        || m_failedEmoteImages.contains(url)) return;

    m_pendingEmoteImages.insert(url);
    QNetworkRequest request{ QUrl(url) };
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    request.setTransferTimeout(10000);
    QNetworkReply* reply = m_emoteNetwork.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        m_pendingEmoteImages.remove(url);
        const QByteArray imageBytes = reply->readAll();
        reply->deleteLater();
        if (imageBytes.size() > 512 * 1024)
        {
            m_failedEmoteImages.insert(url);
            m_failedEmoteOrder.append(url);
            while (m_failedEmoteOrder.size() > 256)
            {
                m_failedEmoteImages.remove(m_failedEmoteOrder.takeFirst());
            }
            return;
        }
        QImage image = QImage::fromData(imageBytes);
        if (image.isNull())
        {
            m_failedEmoteImages.insert(url);
            m_failedEmoteOrder.append(url);
            while (m_failedEmoteOrder.size() > 256)
            {
                m_failedEmoteImages.remove(m_failedEmoteOrder.takeFirst());
            }
            return;
        }

        image = image.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_emoteImages.insert(url, image);
        m_emoteImageOrder.append(url);
        while (m_emoteImageOrder.size() > 128)
        {
            m_emoteImages.remove(m_emoteImageOrder.takeFirst());
        }
        if (!m_emoteRefreshTimer.isActive()) m_emoteRefreshTimer.start();
    });
}

QString ChatWidget::formatMessageBodyHtml(const ChatMessage& msg)
{
    if (msg.emotes.isEmpty())
    {
        return msg.message.toHtmlEscaped();
    }

    QList<ChatEmote> emotes = msg.emotes;
    std::sort(emotes.begin(), emotes.end(), [](const ChatEmote& left, const ChatEmote& right) {
        return left.start < right.start;
    });

    QString html;
    int position = 0;
    for (const ChatEmote& emote : emotes)
    {
        if (emote.start < position || emote.length <= 0 || emote.start + emote.length > msg.message.size()) continue;
        html += msg.message.mid(position, emote.start - position).toHtmlEscaped();
        requestEmoteImage(emote.imageUrl);
        if (m_emoteImages.contains(emote.imageUrl))
        {
            m_chatBrowser->document()->addResource(QTextDocument::ImageResource,
                QUrl(emote.imageUrl), m_emoteImages.value(emote.imageUrl));
            html += QString("<img src=\"%1\" height=\"20\" style=\"vertical-align: middle; margin: 0 2px;\" />")
                .arg(emote.imageUrl);
        }
        else
        {
            html += QString("<span style=\"color: #9CA3AF; font-size: 11px;\">:%1:</span>")
                .arg(emote.altText.isEmpty() ? "emote" : emote.altText.toHtmlEscaped());
        }
        position = emote.start + emote.length;
    }
    if (position < msg.message.size())
    {
        html += msg.message.mid(position).toHtmlEscaped();
    }
    return html;
}

QString ChatWidget::formatMessageHtml(const ChatMessage& msg)
{
    QString timeHtml;
    if (m_style.showTimestamps)
    {
        timeHtml = QString("<span style=\"color: %1; font-size: 11px; margin-right: 6px;\">[%2]</span>")
            .arg(m_style.timestampColor, msg.formattedTime());
    }

    QString badgeHtml;
    if (m_style.showPlatformBadges)
    {
        badgeHtml = getPlatformBadgeHtml(msg.platform);
    }

    QString nameHtml;
    if (m_style.showUsernames)
    {
        QString nameColor = msg.userColor.isEmpty() ? m_style.usernameColor : msg.userColor;
        nameHtml = QString("<span style=\"color: %1; font-weight: bold; margin-right: 6px;\">%2:</span>")
            .arg(nameColor, msg.displayName.toHtmlEscaped());
    }

    QString bodyHtml;
    QString containerStyle;

    if (msg.type == MessageType::DONATION)
    {
        if (m_style.showDonationHighlights)
        {
            containerStyle = "background-color: rgba(245, 158, 11, 0.15); padding: 5px 10px; border-radius: 6px; margin: 3px 0; border-left: 3px solid #F59E0B;";
        }
        QString giftUri = platformLogoDataUri(":/icons/gift.svg");
        bodyHtml = QString("<img src=\"%1\" width=\"13\" height=\"13\" style=\"vertical-align: middle; margin-right: 4px;\" /><span style=\"color: #FBBF24; font-weight: bold;\">[%2 %3]</span> <span style=\"color: #FFFFFF;\">%4</span>")
            .arg(giftUri, QString::number(msg.donationAmount, 'f', 2), msg.currency.toHtmlEscaped(), formatMessageBodyHtml(msg));
    }
    else if (msg.type == MessageType::SUBSCRIPTION)
    {
        if (m_style.showSubscriptionHighlights)
        {
            containerStyle = "background-color: rgba(145, 70, 255, 0.15); padding: 5px 8px; border-radius: 4px; margin: 2px 0; border-left: 3px solid #9146FF;";
        }
        QString starUri = platformLogoDataUri(":/icons/star.svg");
        bodyHtml = QString("<img src=\"%1\" width=\"13\" height=\"13\" style=\"vertical-align: middle; margin-right: 4px;\" /><span style=\"color: #C084FC; font-weight: bold;\">%2</span>")
            .arg(starUri, formatMessageBodyHtml(msg));
    }
    else if (msg.type == MessageType::GIFT)
    {
        if (m_style.showDonationHighlights)
        {
            containerStyle = "background-color: rgba(254, 44, 85, 0.15); padding: 5px 8px; border-radius: 4px; margin: 2px 0; border-left: 3px solid #FE2C55;";
        }
        QString giftUri = platformLogoDataUri(":/icons/gift.svg");
        bodyHtml = QString("<img src=\"%1\" width=\"13\" height=\"13\" style=\"vertical-align: middle; margin-right: 4px;\" /><span style=\"color: #FDA4AF; font-weight: bold;\">%2</span>")
            .arg(giftUri, formatMessageBodyHtml(msg));
    }
    else
    {
        bodyHtml = QString("<span style=\"color: %1;\">%2</span>")
            .arg(m_style.textColor, formatMessageBodyHtml(msg));
    }

    if (!containerStyle.isEmpty())
    {
        return QString("<div style=\"%1\">%2%3%4%5</div>")
            .arg(containerStyle, timeHtml, badgeHtml, nameHtml, bodyHtml);
    }
    else
    {
        return QString("<div style=\"margin: 2px 0; line-height: 1.4;\">%1%2%3%4</div>")
            .arg(timeHtml, badgeHtml, nameHtml, bodyHtml);
    }
}

void ChatWidget::refreshAllMessages()
{
    m_chatBrowser->clear();
    m_renderedCount = 0;

    QList<ChatMessage> messages = ChatManager::instance().getFilteredMessages();
    for (const auto& msg : messages)
    {
        if (!msg.isTransient)
        {
            appendHtmlFormatted(msg);
            m_renderedCount++;
        }
    }

    if (m_autoScroll)
    {
        m_chatBrowser->verticalScrollBar()->setValue(m_chatBrowser->verticalScrollBar()->maximum());
    }
}

void ChatWidget::onFilterChanged()
{
    ChatFilter filter;
    filter.showTwitch = m_chkTwitch->isChecked();
    filter.showYouTube = m_chkYouTube->isChecked();
    filter.showKick = m_chkKick->isChecked();
    filter.showTikTok = m_chkTikTok->isChecked();

    filter.showNormal = m_chkNormal->isChecked();
    filter.showDonations = m_chkDonations->isChecked();
    filter.showSubscriptions = m_chkSubs->isChecked();
    filter.showGifts = m_chkGifts->isChecked();
    filter.showEvents = m_chkEvents->isChecked();

    ChatManager::instance().setFilter(filter);
    refreshAllMessages();
}

void ChatWidget::onClearChatClicked()
{
    ChatManager::instance().clear();
}

void ChatWidget::onAutoScrollToggled(bool checked)
{
    m_autoScroll = checked;
    m_btnAutoScroll->setText(checked ? "Auto-scroll ON" : "Auto-scroll OFF");
}

void ChatWidget::onSimulateMessageClicked()
{
    static const QStringList platforms = { "Twitch", "YouTube", "Kick", "TikTok" };
    static const QStringList users = { "ShadowNinja", "LofiVibes", "GamerPro99", "PixelQueen", "HyperDrift", "CosmicCat" };
    static const QStringList messages = {
        "¡Hola a todos en el stream!",
        "Qué buena jugada GG",
        "¿A qué hora termina el directo?",
        "Saludos desde Colombia y México!",
        "jajaja no puede ser eso estuvo cerca",
        "Suban el volumen del juego porfa",
        "Llegué justo a tiempo"
    };

    int pIdx = QRandomGenerator::global()->bounded(platforms.size());
    int uIdx = QRandomGenerator::global()->bounded(users.size());
    int mIdx = QRandomGenerator::global()->bounded(messages.size());

    ChatMessage msg;
    msg.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    msg.platform = platforms[pIdx];
    msg.username = users[uIdx];
    msg.displayName = users[uIdx];
    msg.message = messages[mIdx];
    msg.timestamp = QDateTime::currentDateTime();
    msg.type = MessageType::NORMAL;

    if (msg.platform == "Twitch") msg.userColor = "#9146FF";
    else if (msg.platform == "YouTube") msg.userColor = "#EF4444";
    else if (msg.platform == "Kick") msg.userColor = "#53FC18";
    else if (msg.platform == "TikTok") msg.userColor = "#FE2C55";

    EventManager::instance().postChatMessage(msg);
}

void ChatWidget::onSimulateDonationClicked()
{
    static const QStringList platforms = { "Twitch", "YouTube", "Kick", "TikTok" };
    static const QStringList donators = { "ElReyDelStream", "DonadorAnonimo", "SuperFan_01", "MegaSupporter", "VipLord" };
    static const QStringList messages = {
        "¡Sigue así crack! Gran directo.",
        "Un saludo especial para la comunidad.",
        "Para la pizza del streamer",
        "Tremenda transmisión, un abrazo.",
        "El mejor directo del día"
    };

    int pIdx = QRandomGenerator::global()->bounded(platforms.size());
    int uIdx = QRandomGenerator::global()->bounded(donators.size());
    int mIdx = QRandomGenerator::global()->bounded(messages.size());

    QString plat = platforms[pIdx];
    QString user = donators[uIdx];
    QString text = messages[mIdx];
    double amounts[] = { 2.50, 5.00, 10.00, 25.00, 50.00, 100.00 };
    double amount = amounts[QRandomGenerator::global()->bounded(6)];

    Donation don;
    don.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    don.platform = plat;
    don.username = user;
    don.displayName = user;
    don.amount = amount;
    don.timestamp = QDateTime::currentDateTime();
    don.message = text;

    ChatMessage msg;
    msg.id = don.id;
    msg.platform = plat;
    msg.username = user;
    msg.displayName = user;
    msg.message = text;
    msg.timestamp = don.timestamp;
    msg.isDonation = true;
    msg.donationAmount = amount;

    if (plat == "Twitch")
    {
        don.type = "Bits / Cheer";
        don.currency = "Bits";
        don.amount = amount * 100;
        msg.currency = "Bits";
        msg.donationAmount = don.amount;
        msg.type = MessageType::DONATION;
        msg.userColor = "#9146FF";
    }
    else if (plat == "YouTube")
    {
        don.type = "Super Chat";
        don.currency = "$";
        msg.currency = "$";
        msg.type = MessageType::DONATION;
        msg.userColor = "#EF4444";
    }
    else if (plat == "Kick")
    {
        don.type = "Kick Subscription";
        don.currency = "$";
        msg.currency = "$";
        msg.type = MessageType::SUBSCRIPTION;
        msg.userColor = "#53FC18";
    }
    else if (plat == "TikTok")
    {
        don.type = "TikTok Gift (Rosa/León)";
        don.currency = "Coins";
        don.amount = amount * 100;
        msg.currency = "Coins";
        msg.donationAmount = don.amount;
        msg.type = MessageType::GIFT;
        msg.userColor = "#FE2C55";
    }

    EventManager::instance().postDonation(don);
    EventManager::instance().postChatMessage(msg);
}

void ChatWidget::onSimulateEventClicked()
{
    static int eventTypeIndex = 0;
    static const QStringList users = { "carlos_garcia", "mariana_live", "alex_streamer", "sofia_gaming", "juan_perez" };
    int uIdx = QRandomGenerator::global()->bounded(users.size());
    QString user = users[uIdx];

    ChatMessage msg;
    msg.id = QString("sim_ev_%1").arg(QDateTime::currentMSecsSinceEpoch());
    msg.platform = "TikTok";
    msg.username = user;
    msg.displayName = user;
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = "#FE2C55";
    msg.isTransient = true;

    int typeChoice = eventTypeIndex % 4;
    eventTypeIndex++;

    if (typeChoice == 0)
    {
        msg.type = MessageType::JOIN;
        msg.message = "¡Se ha unido al directo!";
    }
    else if (typeChoice == 1)
    {
        msg.type = MessageType::SHARE;
        msg.message = "¡Ha compartido el directo!";
    }
    else if (typeChoice == 2)
    {
        msg.type = MessageType::FOLLOW;
        msg.message = "¡Ha comenzado a seguir al streamer!";
    }
    else
    {
        msg.type = MessageType::LIKE;
        if (m_style.showLikeCount)
        {
            msg.message = QString("¡ha enviado %1 likes!").arg(QRandomGenerator::global()->bounded(5, 50));
        }
        else
        {
            msg.message = "¡dio like al directo!";
        }
    }

    EventManager::instance().postChatMessage(msg);
}
