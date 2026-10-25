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
#include <QDialog>
#include <QClipboard>
#include <QGuiApplication>
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

    // Top Filter Bar (Two clean responsive rows)
    m_filterBar = new QWidget(this);
    m_filterBar->setObjectName("FilterBar");
    m_filterBar->setStyleSheet(
        "QWidget#FilterBar { background-color: #161A23; border: 1px solid rgba(255,255,255,0.06); border-radius: 8px; }"
        "QLabel { background: transparent; border: none; }"
    );
    auto* filterBar = m_filterBar;
    auto* barLayout = new QVBoxLayout(m_filterBar);
    barLayout->setContentsMargins(10, 8, 10, 8);
    barLayout->setSpacing(8);

    // Row 1: Platforms & Message Types
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(6);

    auto* lblFilterIcon = new QLabel(filterBar);
    lblFilterIcon->setPixmap(QIcon(":/icons/filter.svg").pixmap(14, 14));
    auto* lblFilter = new QLabel("Filtros:", filterBar);
    lblFilter->setStyleSheet("font-weight: 700; color: #94A3B8; font-size: 11px;");
    row1->addWidget(lblFilterIcon);
    row1->addWidget(lblFilter);

    // Platform toggle buttons
    auto createPlatformButton = [this, filterBar](const QString& name, const QString& color, const QString& iconPath) -> QPushButton* {
        auto* btn = new QPushButton(filterBar);
        btn->setCheckable(true);
        btn->setChecked(true);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(16, 16));
        btn->setFixedSize(30, 26);
        btn->setToolTip(QString("%1: Click para alternar").arg(name));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: %1; border-radius: 5px; border: 1px solid rgba(255,255,255,0.25); }"
            "QPushButton:hover { border-color: rgba(255,255,255,0.5); }"
            "QPushButton:!checked { background: rgba(255,255,255,0.05); border-color: rgba(255,255,255,0.06); }"
        ).arg(color));
        connect(btn, &QPushButton::toggled, this, &ChatWidget::onFilterChanged);
        return btn;
    };

    m_chkTwitch = createPlatformButton("Twitch", "#9146FF", ":/icons/twitch.svg");
    m_chkYouTube = createPlatformButton("YouTube", "#EF4444", ":/icons/youtube.svg");
    m_chkKick = createPlatformButton("Kick", "#53FC18", ":/icons/kick.svg");
    m_chkTikTok = createPlatformButton("TikTok", "#FE2C55", ":/icons/tiktok.svg");

    row1->addWidget(m_chkTwitch);
    row1->addWidget(m_chkYouTube);
    row1->addWidget(m_chkKick);
    row1->addWidget(m_chkTikTok);

    auto* sep1 = new QFrame(filterBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: rgba(255,255,255,0.12);");
    row1->addWidget(sep1);

    // Type filter pills
    auto createTypeButton = [this, filterBar](const QString& name, const QString& tip) -> QPushButton* {
        auto* btn = new QPushButton(name, filterBar);
        btn->setCheckable(true);
        btn->setChecked(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(26);
        btn->setToolTip(tip);
        btn->setStyleSheet(
            "QPushButton { background: rgba(37, 99, 235, 0.25); color: #93C5FD; border: 1px solid rgba(59, 130, 246, 0.4); border-radius: 6px; padding: 0 8px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: rgba(37, 99, 235, 0.4); color: #FFFFFF; }"
            "QPushButton:!checked { background: rgba(255, 255, 255, 0.04); color: #64748B; border-color: rgba(255, 255, 255, 0.08); }"
        );
        connect(btn, &QPushButton::toggled, this, &ChatWidget::onFilterChanged);
        return btn;
    };

    m_chkNormal = createTypeButton("Chat", "Mensajes de chat normales");
    m_chkDonations = createTypeButton("Donaciones", "Super Chats, Bits y Donaciones");
    m_chkSubs = createTypeButton("Subs", "Suscripciones y renovaciones");
    m_chkGifts = createTypeButton("Gifts", "Regalos y sub gifts");
    m_chkEvents = createTypeButton("Eventos", "Notificaciones de personas que se unen, dan like o comparten");

    row1->addWidget(m_chkNormal);
    row1->addWidget(m_chkDonations);
    row1->addWidget(m_chkSubs);
    row1->addWidget(m_chkGifts);
    row1->addWidget(m_chkEvents);
    row1->addStretch();

    // Row 2: Action Tools (Auto-scroll, Limpiar, Abrir Web, Simular)
    auto* row2 = new QHBoxLayout();
    row2->setSpacing(8);

    m_btnAutoScroll = new QPushButton("Auto-scroll ON", filterBar);
    m_btnAutoScroll->setCheckable(true);
    m_btnAutoScroll->setChecked(true);
    m_btnAutoScroll->setFixedHeight(26);
    m_btnAutoScroll->setCursor(Qt::PointingHandCursor);
    m_btnAutoScroll->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none; border-radius: 5px; padding: 0 10px; font-size: 11px; font-weight: 700; }"
        "QPushButton:!checked { background: #374151; color: #94A3B8; }"
    );
    connect(m_btnAutoScroll, &QPushButton::toggled, this, &ChatWidget::onAutoScrollToggled);
    row2->addWidget(m_btnAutoScroll);

    m_btnClear = new QPushButton("Limpiar", filterBar);
    m_btnClear->setIcon(QIcon(":/icons/trash.svg"));
    m_btnClear->setIconSize(QSize(12, 12));
    m_btnClear->setFixedHeight(26);
    m_btnClear->setCursor(Qt::PointingHandCursor);
    m_btnClear->setStyleSheet("QPushButton { background: #374151; color: #E5E7EB; border: none; border-radius: 5px; padding: 0 10px; font-size: 11px; font-weight: 500; } QPushButton:hover { background: #4B5563; }");
    connect(m_btnClear, &QPushButton::clicked, this, &ChatWidget::onClearChatClicked);
    row2->addWidget(m_btnClear);

    m_btnOpenWeb = new QPushButton("Abrir Web", filterBar);
    m_btnOpenWeb->setIcon(QIcon(":/icons/globe.svg"));
    m_btnOpenWeb->setIconSize(QSize(12, 12));
    m_btnOpenWeb->setFixedHeight(26);
    m_btnOpenWeb->setCursor(Qt::PointingHandCursor);
    m_btnOpenWeb->setToolTip("Abre la interfaz web moderna en tu navegador o comparte con tu móvil");
    m_btnOpenWeb->setStyleSheet("QPushButton { background: #0D9488; color: white; border: none; border-radius: 5px; padding: 0 10px; font-size: 11px; font-weight: 700; } QPushButton:hover { background: #0F766E; }");
    connect(m_btnOpenWeb, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("http://localhost:8080"));
    });
    row2->addWidget(m_btnOpenWeb);

    m_btnSimulate = new QPushButton("Simular", filterBar);
    m_btnSimulate->setIcon(QIcon(":/icons/dice.svg"));
    m_btnSimulate->setIconSize(QSize(12, 12));
    m_btnSimulate->setFixedHeight(26);
    m_btnSimulate->setToolTip("Simular mensajes, donaciones y eventos de directo");
    m_btnSimulate->setCursor(Qt::PointingHandCursor);
    m_btnSimulate->setStyleSheet(
        "QPushButton { background: #374151; color: #E5E7EB; border: none; border-radius: 5px; padding: 0 10px; font-size: 11px; font-weight: 500; }"
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
    row2->addWidget(m_btnSimulate);

    row2->addStretch();

    barLayout->addLayout(row1);
    barLayout->addLayout(row2);
    mainLayout->addWidget(m_filterBar);

    // Chat Browser
    m_chatBrowser->setOpenExternalLinks(false);
    m_chatBrowser->setReadOnly(true);
    m_chatBrowser->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #12141A;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  color: #F3F4F6;"
        "}"
        "a { text-decoration: none; color: inherit; }"
    );
    connect(m_chatBrowser, &QTextBrowser::anchorClicked, this, &ChatWidget::onMessageAnchorClicked);
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

    // Only keep at most 1 visible toast at any time so the chat isn't squished or scrolled up
    while (m_transientLayout->count() > 0)
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

    // Auto-scroll to keep bottom-most chat comment completely visible
    if (m_autoScroll)
    {
        QTimer::singleShot(25, this, [this]() {
            m_chatBrowser->verticalScrollBar()->setValue(m_chatBrowser->verticalScrollBar()->maximum());
        });
    }

    // Auto-dismiss using configured duration
    int durationMs = qBound(1000, static_cast<int>((m_style.transientDurationSec > 0.0 ? m_style.transientDurationSec : 3.5) * 1000), 20000);
    QTimer::singleShot(durationMs, toast, [this, toast]() {
        toast->deleteLater();
        QTimer::singleShot(40, this, [this]() {
            if (m_transientLayout->count() == 0)
            {
                m_transientContainer->hide();
            }
            if (m_autoScroll)
            {
                m_chatBrowser->verticalScrollBar()->setValue(m_chatBrowser->verticalScrollBar()->maximum());
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
        nameHtml = QString("<span style=\"color: %1; font-weight: bold;\">%2:&nbsp;</span>")
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

    QString content = QString("%1%2%3%4").arg(timeHtml, badgeHtml, nameHtml, bodyHtml);
    QString link = QString("<a href=\"msg:%1\" style=\"text-decoration: none; color: inherit;\">%2</a>").arg(msg.id, content);

    if (!containerStyle.isEmpty())
    {
        return QString("<div style=\"%1\">%2</div>")
            .arg(containerStyle, link);
    }
    else
    {
        return QString("<div style=\"margin: 2px 0; line-height: 1.4;\">%1</div>")
            .arg(link);
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

void ChatWidget::onMessageAnchorClicked(const QUrl& url)
{
    if (url.scheme() == "msg")
    {
        QString msgId = url.path();
        if (msgId.isEmpty())
        {
            msgId = url.toString().mid(4); // strip "msg:"
        }
        ChatMessage msg = ChatManager::instance().getMessageById(msgId);
        if (!msg.id.isEmpty())
        {
            showMessageDetailDialog(msg);
        }
    }
    else if (url.scheme() == "http" || url.scheme() == "https")
    {
        QDesktopServices::openUrl(url);
    }
}

void ChatWidget::showMessageDetailDialog(const ChatMessage& msg)
{
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Detalle del Mensaje");
    dialog->setModal(true);
    dialog->setMinimumWidth(380);
    dialog->setMaximumWidth(520);
    dialog->setStyleSheet(
        "QDialog { background-color: #111827; border: 1px solid #374151; border-radius: 12px; }"
        "QLabel { background: transparent; border: none; }"
    );

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    // Top Row: Platform Icon + Badge + Timestamp
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    QString iconPath = ":/icons/tiktok.svg";
    QString platColor = "#FE2C55";
    if (msg.platform.compare("Twitch", Qt::CaseInsensitive) == 0) { iconPath = ":/icons/twitch.svg"; platColor = "#9146FF"; }
    else if (msg.platform.compare("YouTube", Qt::CaseInsensitive) == 0) { iconPath = ":/icons/youtube.svg"; platColor = "#EF4444"; }
    else if (msg.platform.compare("Kick", Qt::CaseInsensitive) == 0) { iconPath = ":/icons/kick.svg"; platColor = "#53FC18"; }

    auto* platIcon = new QLabel(dialog);
    platIcon->setPixmap(QIcon(iconPath).pixmap(18, 18));
    topRow->addWidget(platIcon);

    auto* platName = new QLabel(msg.platform.toUpper(), dialog);
    platName->setStyleSheet(QString("font-size: 12px; font-weight: 800; color: %1; letter-spacing: 0.8px;").arg(platColor));
    topRow->addWidget(platName);

    topRow->addStretch();

    auto* timeLbl = new QLabel(msg.formattedTime("HH:mm:ss - dd/MM/yyyy"), dialog);
    timeLbl->setStyleSheet("font-family: monospace; font-size: 11px; color: #94A3B8;");
    topRow->addWidget(timeLbl);
    layout->addLayout(topRow);

    // User Name (Large)
    QString userColor = msg.userColor.isEmpty() ? platColor : msg.userColor;
    auto* userLbl = new QLabel(msg.displayName.isEmpty() ? msg.username : msg.displayName, dialog);
    userLbl->setStyleSheet(QString("font-size: 22px; font-weight: 800; color: %1;").arg(userColor));
    userLbl->setWordWrap(true);
    userLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(userLbl);

    // If Donation / Gift / Sub Highlight
    if (msg.isDonation || msg.type == MessageType::DONATION)
    {
        auto* donFrame = new QFrame(dialog);
        donFrame->setStyleSheet("background: rgba(245, 158, 11, 0.15); border: 1px solid rgba(245, 158, 11, 0.4); border-radius: 6px; padding: 6px 10px;");
        auto* donLayout = new QHBoxLayout(donFrame);
        donLayout->setContentsMargins(6, 4, 6, 4);
        auto* donIcon = new QLabel(donFrame);
        donIcon->setPixmap(QIcon(":/icons/gift.svg").pixmap(16, 16));
        auto* donText = new QLabel(QString("Donación: %1 %2").arg(QString::number(msg.donationAmount, 'f', 2), msg.currency), donFrame);
        donText->setStyleSheet("color: #FBBF24; font-weight: 700; font-size: 13px;");
        donLayout->addWidget(donIcon);
        donLayout->addWidget(donText);
        donLayout->addStretch();
        layout->addWidget(donFrame);
    }
    else if (msg.type == MessageType::GIFT)
    {
        auto* giftFrame = new QFrame(dialog);
        giftFrame->setStyleSheet("background: rgba(254, 44, 85, 0.15); border: 1px solid rgba(254, 44, 85, 0.4); border-radius: 6px; padding: 6px 10px;");
        auto* giftLayout = new QHBoxLayout(giftFrame);
        giftLayout->setContentsMargins(6, 4, 6, 4);
        auto* giftIcon = new QLabel(giftFrame);
        giftIcon->setPixmap(QIcon(":/icons/gift.svg").pixmap(16, 16));
        auto* giftText = new QLabel(QString("Regalo: %1 (x%2)").arg(msg.giftName.isEmpty() ? "Regalo" : msg.giftName).arg(msg.giftCount), giftFrame);
        giftText->setStyleSheet("color: #FDA4AF; font-weight: 700; font-size: 13px;");
        giftLayout->addWidget(giftIcon);
        giftLayout->addWidget(giftText);
        giftLayout->addStretch();
        layout->addWidget(giftFrame);
    }
    else if (msg.type == MessageType::SUBSCRIPTION)
    {
        auto* subFrame = new QFrame(dialog);
        subFrame->setStyleSheet("background: rgba(145, 70, 255, 0.15); border: 1px solid rgba(145, 70, 255, 0.4); border-radius: 6px; padding: 6px 10px;");
        auto* subLayout = new QHBoxLayout(subFrame);
        subLayout->setContentsMargins(6, 4, 6, 4);
        auto* subIcon = new QLabel(subFrame);
        subIcon->setPixmap(QIcon(":/icons/star.svg").pixmap(16, 16));
        auto* subText = new QLabel("Suscripción al canal", subFrame);
        subText->setStyleSheet("color: #C084FC; font-weight: 700; font-size: 13px;");
        subLayout->addWidget(subIcon);
        subLayout->addWidget(subText);
        subLayout->addStretch();
        layout->addWidget(subFrame);
    }

    // Message Content Box
    auto* msgBox = new QFrame(dialog);
    msgBox->setStyleSheet("background-color: #1F2937; border: 1px solid #374151; border-radius: 8px; padding: 12px;");
    auto* msgBoxLayout = new QVBoxLayout(msgBox);
    msgBoxLayout->setContentsMargins(10, 10, 10, 10);
    msgBoxLayout->setSpacing(6);

    auto* msgHeader = new QLabel("MENSAJE", msgBox);
    msgHeader->setStyleSheet("font-size: 10px; font-weight: 700; color: #64748B; letter-spacing: 0.8px;");
    msgBoxLayout->addWidget(msgHeader);

    auto* msgText = new QLabel(msg.message, msgBox);
    msgText->setStyleSheet("font-size: 15px; color: #F3F4F6; line-height: 1.4;");
    msgText->setWordWrap(true);
    msgText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    msgBoxLayout->addWidget(msgText);
    layout->addWidget(msgBox);

    // Actions Row (Copy Message, Copy User, Close)
    auto* actRow = new QHBoxLayout();
    actRow->setSpacing(8);

    auto* btnCopyMsg = new QPushButton("Copiar Mensaje", dialog);
    btnCopyMsg->setIcon(QIcon(":/icons/copy.svg"));
    btnCopyMsg->setFixedHeight(32);
    btnCopyMsg->setCursor(Qt::PointingHandCursor);
    btnCopyMsg->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none; border-radius: 6px; padding: 0 14px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #1D4ED8; }"
    );
    connect(btnCopyMsg, &QPushButton::clicked, dialog, [btnCopyMsg, msg]() {
        QGuiApplication::clipboard()->setText(msg.message);
        btnCopyMsg->setText("¡Copiado!");
        QTimer::singleShot(1500, btnCopyMsg, [btnCopyMsg]() {
            btnCopyMsg->setText("Copiar Mensaje");
        });
    });
    actRow->addWidget(btnCopyMsg);

    actRow->addStretch();

    auto* btnClose = new QPushButton("Cerrar", dialog);
    btnClose->setFixedHeight(32);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setStyleSheet(
        "QPushButton { background: #374151; color: #E5E7EB; border: none; border-radius: 6px; padding: 0 14px; font-weight: 500; font-size: 12px; }"
        "QPushButton:hover { background: #4B5563; }"
    );
    connect(btnClose, &QPushButton::clicked, dialog, &QDialog::accept);
    actRow->addWidget(btnClose);

    layout->addLayout(actRow);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}
