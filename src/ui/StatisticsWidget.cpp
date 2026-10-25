#include "StatisticsWidget.h"
#include "EventManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QIcon>

StatisticsWidget::StatisticsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    updateViewerCounts();

    connect(&EventManager::instance(), &EventManager::viewerCountUpdated, this, &StatisticsWidget::updateViewerCounts);
    connect(&EventManager::instance(), &EventManager::platformStatusChanged, this, &StatisticsWidget::updateViewerCounts);
}

void StatisticsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    auto* titleLayout = new QHBoxLayout();
    auto* titleIcon = new QLabel(this);
    titleIcon->setPixmap(QIcon(":/icons/stats.svg").pixmap(20, 20));
    auto* titleLabel = new QLabel("ESTADÍSTICAS Y AUDIENCIA EN TIEMPO REAL", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;");
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    mainLayout->addLayout(titleLayout);

    // Big Total Card
    auto* totalCard = new QFrame(this);
    totalCard->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1E3A8A, stop:1 #2563EB); border-radius: 12px; padding: 16px;");
    auto* totalLayout = new QVBoxLayout(totalCard);
    totalLayout->setContentsMargins(20, 16, 20, 16);

    auto* lblTotalTitle = new QLabel("AUDIENCIA TOTAL MULTISTREAM", totalCard);
    lblTotalTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #93C5FD; letter-spacing: 1px;");

    m_lblTotalViewers = new QLabel("0 Viewers", totalCard);
    m_lblTotalViewers->setStyleSheet("font-size: 36px; font-weight: bold; color: #FFFFFF;");

    m_lblConnectedCount = new QLabel("0 de 4 plataformas conectadas", totalCard);
    m_lblConnectedCount->setStyleSheet("font-size: 12px; color: #E0E7FF;");

    totalLayout->addWidget(lblTotalTitle);
    totalLayout->addWidget(m_lblTotalViewers);
    totalLayout->addWidget(m_lblConnectedCount);

    mainLayout->addWidget(totalCard);

    // Grid of Platforms with SVG icons
    auto* grid = new QGridLayout();
    grid->setSpacing(16);

    auto* cardTwitch = createStatCard("Twitch", ":/icons/twitch.svg", "#9146FF", m_lblTwitchViewers, m_lblTwitchStatus);
    auto* cardYouTube = createStatCard("YouTube", ":/icons/youtube.svg", "#EF4444", m_lblYouTubeViewers, m_lblYouTubeStatus);
    auto* cardKick = createStatCard("Kick", ":/icons/kick.svg", "#53FC18", m_lblKickViewers, m_lblKickStatus);
    auto* cardTikTok = createStatCard("TikTok", ":/icons/tiktok.svg", "#FE2C55", m_lblTikTokViewers, m_lblTikTokStatus);

    grid->addWidget(cardTwitch, 0, 0);
    grid->addWidget(cardYouTube, 0, 1);
    grid->addWidget(cardKick, 1, 0);
    grid->addWidget(cardTikTok, 1, 1);

    mainLayout->addLayout(grid);
    mainLayout->addStretch();
}

QWidget* StatisticsWidget::createStatCard(const QString& title, const QString& iconPath, const QString& color, QLabel*& outValueLabel, QLabel*& outStatusLabel)
{
    auto* card = new QFrame(this);
    card->setStyleSheet("background-color: #1A1D24; border-radius: 10px; border: 1px solid #2D3748; padding: 12px;");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* topLayout = new QHBoxLayout();
    auto* lblIcon = new QLabel(card);
    lblIcon->setPixmap(QIcon(iconPath).pixmap(20, 20));
    
    auto* lblTitle = new QLabel(title, card);
    lblTitle->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;").arg(color));

    outStatusLabel = new QLabel("OFFLINE", card);
    outStatusLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #6B7280;");

    topLayout->addWidget(lblIcon);
    topLayout->addWidget(lblTitle);
    topLayout->addStretch();
    topLayout->addWidget(outStatusLabel);

    outValueLabel = new QLabel("No disponible", card);
    outValueLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #FFFFFF;");

    layout->addLayout(topLayout);
    layout->addWidget(outValueLabel);

    return card;
}

void StatisticsWidget::updateViewerCounts()
{
    auto updateItem = [](const QString& platform, QLabel* valLbl, QLabel* statusLbl) {
        int viewers = EventManager::instance().getViewerCount(platform);
        PlatformStatus status = EventManager::instance().getPlatformStatus(platform);

        statusLbl->setText(platformStatusToString(status));
        statusLbl->setStyleSheet(QString("font-size: 11px; font-weight: bold; color: %1;").arg(platformStatusColorHex(status)));

        if (viewers >= 0)
        {
            valLbl->setText(QString("%1 viewers").arg(QLocale().toString(viewers)));
        }
        else
        {
            valLbl->setText(status == PlatformStatus::ONLINE ? "Calculando..." : "No disponible");
        }
    };

    updateItem("Twitch", m_lblTwitchViewers, m_lblTwitchStatus);
    updateItem("YouTube", m_lblYouTubeViewers, m_lblYouTubeStatus);
    updateItem("Kick", m_lblKickViewers, m_lblKickStatus);
    updateItem("TikTok", m_lblTikTokViewers, m_lblTikTokStatus);

    int total = EventManager::instance().getTotalViewerCount();
    if (total >= 0)
    {
        m_lblTotalViewers->setText(QString("%1 Viewers").arg(QLocale().toString(total)));
    }
    else
    {
        m_lblTotalViewers->setText("-- Viewers");
    }

    int onlineCount = 0;
    QStringList platforms = { "Twitch", "YouTube", "Kick", "TikTok" };
    for (const auto& p : platforms)
    {
        if (EventManager::instance().getPlatformStatus(p) == PlatformStatus::ONLINE)
        {
            onlineCount++;
        }
    }
    m_lblConnectedCount->setText(QString("%1 de 4 plataformas en directo").arg(onlineCount));
}
