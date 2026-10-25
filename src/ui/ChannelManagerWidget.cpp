#include "ChannelManagerWidget.h"
#include "ConfigManager.h"
#include "ConnectionManager.h"
#include "EventManager.h"
#include "Logger.h"
#include "TikTokPlatform.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QIcon>

ChannelManagerWidget::ChannelManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    loadFromConfig();

    connect(&EventManager::instance(), &EventManager::platformStatusChanged, this, &ChannelManagerWidget::updateStatus);
    connect(&EventManager::instance(), &EventManager::viewerCountUpdated, this, [this](const QString& platform, int count, int total) {
        Q_UNUSED(total);
        updateViewers(platform, count);
    });
}

void ChannelManagerWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // Header
    auto* headerLayout = new QHBoxLayout();
    auto* titleIcon = new QLabel(this);
    titleIcon->setPixmap(QIcon(":/icons/channels.svg").pixmap(20, 20));
    auto* titleLabel = new QLabel("GESTIÓN DE CANALES Y PLATAFORMAS", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;");
    headerLayout->addWidget(titleIcon);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_btnSaveAll = new QPushButton("Guardar Canales", this);
    m_btnSaveAll->setIcon(QIcon(":/icons/save.svg"));
    m_btnSaveAll->setIconSize(QSize(16, 16));
    m_btnSaveAll->setCursor(Qt::PointingHandCursor);
    m_btnSaveAll->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; font-weight: bold; border-radius: 6px; padding: 8px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #1D4ED8; }"
    );
    connect(m_btnSaveAll, &QPushButton::clicked, this, &ChannelManagerWidget::onSaveAllClicked);
    headerLayout->addWidget(m_btnSaveAll);
    mainLayout->addLayout(headerLayout);

    // Scrollable area for cards
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");

    auto* container = new QWidget();
    auto* gridLayout = new QGridLayout(container);
    gridLayout->setSpacing(16);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    // Platform Cards with SVG icons
    auto* cardTwitch = createPlatformCard("Twitch", "#9146FF", ":/icons/twitch.svg", "Usuario o canal (ej. shroud, ibai)");
    auto* cardYouTube = createPlatformCard("YouTube", "#EF4444", ":/icons/youtube.svg", "@canal o ID de video (ej. @LofiGirl)", "API Key opcional:");
    auto* cardKick = createPlatformCard("Kick", "#53FC18", ":/icons/kick.svg", "Slug del canal (ej. xqc, westcol)", "Chatroom ID opcional:");
    auto* cardTikTok = createPlatformCard("TikTok", "#FE2C55", ":/icons/tiktok.svg", "Usuario sin @ o URL (ej. streamer)");

    gridLayout->addWidget(cardTwitch, 0, 0);
    gridLayout->addWidget(cardYouTube, 0, 1);
    gridLayout->addWidget(cardKick, 1, 0);
    gridLayout->addWidget(cardTikTok, 1, 1);

    scrollArea->setWidget(container);
    mainLayout->addWidget(scrollArea, 1);
}

QWidget* ChannelManagerWidget::createPlatformCard(const QString& platform, const QString& color, const QString& iconPath, const QString& placeholder, const QString& extraLabel)
{
    auto* card = new QFrame(this);
    card->setStyleSheet(QString(
        "QFrame {"
        "  background-color: #1A1D24;"
        "  border-radius: 10px;"
        "  border: 1px solid #2D3748;"
        "}"
    ));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(10);

    // Top Header: Icon + Name + Status Badge + Enable Checkbox
    auto* topLayout = new QHBoxLayout();
    
    auto* iconLabel = new QLabel(card);
    iconLabel->setPixmap(QIcon(iconPath).pixmap(18, 18));

    auto* nameLabel = new QLabel(platform.toUpper(), card);
    nameLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 13px;").arg(color));

    auto* lblStatus = new QLabel("● OFFLINE", card);
    lblStatus->setStyleSheet("font-size: 11px; font-weight: bold; color: #6B7280;");

    auto* chkEnabled = new QCheckBox("Habilitado", card);
    chkEnabled->setChecked(true);
    chkEnabled->setStyleSheet("color: #9CA3AF; font-size: 11px;");

    topLayout->addWidget(iconLabel);
    topLayout->addWidget(nameLabel);
    topLayout->addWidget(lblStatus);
    topLayout->addStretch();
    topLayout->addWidget(chkEnabled);
    cardLayout->addLayout(topLayout);

    // Channel Input Field
    auto* lblInput = new QLabel("Canal / Usuario:", card);
    lblInput->setStyleSheet("color: #D1D5DB; font-size: 12px; font-weight: 500;");
    cardLayout->addWidget(lblInput);

    auto* edtChannel = new QLineEdit(card);
    edtChannel->setPlaceholderText(placeholder);
    edtChannel->setStyleSheet(
        "QLineEdit {"
        "  background-color: #111827;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 8px 10px;"
        "  color: #FFFFFF;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #3B82F6;"
        "}"
    );
    cardLayout->addWidget(edtChannel);

    QLineEdit* edtExtra = nullptr;
    if (!extraLabel.isEmpty())
    {
        auto* lblExtra = new QLabel(extraLabel, card);
        lblExtra->setStyleSheet("color: #9CA3AF; font-size: 11px;");
        cardLayout->addWidget(lblExtra);

        edtExtra = new QLineEdit(card);
        edtExtra->setPlaceholderText("Dejar en blanco para autodetección pública");
        edtExtra->setStyleSheet(
            "QLineEdit {"
            "  background-color: #111827;"
            "  border: 1px solid #374151;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "  color: #D1D5DB;"
            "  font-size: 11px;"
            "}"
        );
        cardLayout->addWidget(edtExtra);
    }

    // Bottom Stats & Action
    auto* bottomLayout = new QHBoxLayout();
    auto* eyeIcon = new QLabel(card);
    eyeIcon->setPixmap(QIcon(":/icons/eye.svg").pixmap(14, 14));
    
    auto* lblViewers = new QLabel("Viewers: --", card);
    lblViewers->setStyleSheet("color: #38BDF8; font-size: 12px; font-weight: bold;");

    auto* btnConnect = new QPushButton("Conectar", card);
    btnConnect->setCursor(Qt::PointingHandCursor);
    btnConnect->setStyleSheet(
        "QPushButton { background: #10B981; color: white; font-weight: bold; border-radius: 6px; padding: 6px 14px; font-size: 11px; }"
        "QPushButton:hover { background: #059669; }"
    );
    connect(btnConnect, &QPushButton::clicked, this, [this, platform]() {
        onTogglePlatformClicked(platform);
    });

    bottomLayout->addWidget(eyeIcon);
    bottomLayout->addWidget(lblViewers);
    bottomLayout->addStretch();

    if (platform == "TikTok")
    {
        auto* btnBrowser = new QPushButton("👁️ Navegador", card);
        btnBrowser->setCursor(Qt::PointingHandCursor);
        btnBrowser->setToolTip("Muestra u oculta la ventana de Chrome en segundo plano (para resolver verificación o captcha si es necesario)");
        btnBrowser->setStyleSheet(
            "QPushButton { background: #374151; color: #D1D5DB; font-weight: bold; border-radius: 6px; padding: 6px 10px; font-size: 11px; }"
            "QPushButton:hover { background: #4B5563; color: white; }"
        );
        connect(btnBrowser, &QPushButton::clicked, this, [btnBrowser]() {
            auto* tt = dynamic_cast<TikTokPlatform*>(ConnectionManager::instance().getPlatform("TikTok"));
            if (tt)
            {
                bool next = !tt->isBrowserVisible();
                tt->setBrowserVisible(next);
                btnBrowser->setText(next ? "Ocultar" : "👁️ Navegador");
            }
        });
        bottomLayout->addWidget(btnBrowser);
    }

    bottomLayout->addWidget(btnConnect);
    cardLayout->addLayout(bottomLayout);

    PlatformCardWidgets widgets;
    widgets.platform = platform;
    widgets.edtChannel = edtChannel;
    widgets.edtExtra = edtExtra;
    widgets.lblStatus = lblStatus;
    widgets.lblViewers = lblViewers;
    widgets.btnToggleConnect = btnConnect;
    widgets.chkEnabled = chkEnabled;
    m_cards[platform] = widgets;

    return card;
}

void ChannelManagerWidget::loadFromConfig()
{
    QList<ChannelConfig> channels = ConfigManager::instance().getChannels();
    for (const auto& cfg : channels)
    {
        if (m_cards.contains(cfg.platform))
        {
            auto& w = m_cards[cfg.platform];
            w.edtChannel->setText(cfg.channelName);
            if (w.edtExtra)
            {
                w.edtExtra->setText(cfg.extraKey);
            }
            w.chkEnabled->setChecked(cfg.enabled);
        }
    }
}

void ChannelManagerWidget::onSaveAllClicked()
{
    QList<ChannelConfig> updatedList;
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it)
    {
        ChannelConfig cfg;
        cfg.platform = it.key();
        cfg.channelName = it.value().edtChannel->text().trimmed();
        cfg.enabled = it.value().chkEnabled->isChecked();
        if (it.value().edtExtra)
        {
            cfg.extraKey = it.value().edtExtra->text().trimmed();
        }
        updatedList.append(cfg);
    }

    ConfigManager::instance().setChannels(updatedList);
    Logger::instance().info("ChannelManager", "Canales guardados correctamente.");
}

void ChannelManagerWidget::onTogglePlatformClicked(const QString& platform)
{
    if (!m_cards.contains(platform)) return;

    auto& w = m_cards[platform];
    onSaveAllClicked();

    BasePlatform* p = ConnectionManager::instance().getPlatform(platform);
    if (!p) return;

    if (p->isOnline() || p->getStatus() == PlatformStatus::CONNECTING)
    {
        p->disconnectPlatform();
        w.btnToggleConnect->setText("Conectar");
        w.btnToggleConnect->setStyleSheet(
            "QPushButton { background: #10B981; color: white; font-weight: bold; border-radius: 6px; padding: 6px 14px; font-size: 11px; }"
            "QPushButton:hover { background: #059669; }"
        );
    }
    else
    {
        p->connectPlatform();
        w.btnToggleConnect->setText("Desconectar");
        w.btnToggleConnect->setStyleSheet(
            "QPushButton { background: #EF4444; color: white; font-weight: bold; border-radius: 6px; padding: 6px 14px; font-size: 11px; }"
            "QPushButton:hover { background: #DC2626; }"
        );
    }
}

void ChannelManagerWidget::updateStatus(const QString& platform, PlatformStatus status, const QString& detail)
{
    Q_UNUSED(detail);
    if (!m_cards.contains(platform)) return;

    auto& w = m_cards[platform];
    QString color = platformStatusColorHex(status);
    QString text = platformStatusToString(status);

    w.lblStatus->setText(QString("● %1").arg(text));
    w.lblStatus->setStyleSheet(QString("font-size: 11px; font-weight: bold; color: %1;").arg(color));

    if (status == PlatformStatus::ONLINE)
    {
        w.btnToggleConnect->setText("Desconectar");
        w.btnToggleConnect->setStyleSheet(
            "QPushButton { background: #EF4444; color: white; font-weight: bold; border-radius: 6px; padding: 6px 14px; font-size: 11px; }"
            "QPushButton:hover { background: #DC2626; }"
        );
    }
    else if (status == PlatformStatus::OFFLINE)
    {
        w.btnToggleConnect->setText("Conectar");
        w.btnToggleConnect->setStyleSheet(
            "QPushButton { background: #10B981; color: white; font-weight: bold; border-radius: 6px; padding: 6px 14px; font-size: 11px; }"
            "QPushButton:hover { background: #059669; }"
        );
    }
}

void ChannelManagerWidget::updateViewers(const QString& platform, int count)
{
    if (!m_cards.contains(platform)) return;

    auto& w = m_cards[platform];
    if (count >= 0)
    {
        w.lblViewers->setText(QString("Viewers: %1").arg(QLocale().toString(count)));
    }
    else
    {
        w.lblViewers->setText("Viewers: --");
    }
}
