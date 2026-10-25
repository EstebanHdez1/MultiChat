#include "Sidebar.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QLocale>
#include <QIcon>

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
    , m_buttonGroup(new QButtonGroup(this))
{
    setupUi();
}

void Sidebar::setupUi()
{
    setObjectName("SidebarWidget");
    setFixedWidth(220);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 14, 10, 14);
    mainLayout->setSpacing(6);

    // Title / Brand
    auto* brandLayout = new QHBoxLayout();
    auto* brandLogo = new QLabel(this);
    brandLogo->setPixmap(QIcon(":/icons/bolt.svg").pixmap(20, 20));
    
    m_brandTitle = new QLabel("MULTI-CHAT", this);
    m_brandTitle->setStyleSheet("font-size: 15px; font-weight: 800; color: #FFFFFF; letter-spacing: 1.2px;");
    m_btnCollapse = new QPushButton(this);
    m_btnCollapse->setIcon(QIcon(":/icons/chevron-left.svg"));
    m_btnCollapse->setIconSize(QSize(16, 16));
    m_btnCollapse->setToolTip("Contraer menú lateral");
    m_btnCollapse->setCursor(Qt::PointingHandCursor);
    m_btnCollapse->setFixedSize(28, 28);
    m_btnCollapse->setStyleSheet(
        "QPushButton { background: rgba(255, 255, 255, 0.04); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 6px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.1); border-color: rgba(255, 255, 255, 0.2); }"
    );
    connect(m_btnCollapse, &QPushButton::clicked, this, &Sidebar::onToggleCollapsed);
    brandLayout->addWidget(brandLogo);
    brandLayout->addWidget(m_brandTitle);
    brandLayout->addStretch();
    brandLayout->addWidget(m_btnCollapse);
    mainLayout->addLayout(brandLayout);

    mainLayout->addSpacing(6);

    // Navigation buttons
    m_buttonGroup->setExclusive(true);
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &Sidebar::onButtonClicked);

    auto* btnChat = createNavButton(":/icons/chat.svg", "Multichat", 0);
    auto* btnChannels = createNavButton(":/icons/channels.svg", "Canales", 1);
    auto* btnWebServer = createNavButton(":/icons/globe.svg", "Modo Web / Móvil", 2);
    auto* btnDonations = createNavButton(":/icons/trophy.svg", "Top Donaciones", 3);
    auto* btnStats = createNavButton(":/icons/stats.svg", "Estadísticas", 4);
    auto* btnStyle = createNavButton(":/icons/style.svg", "Estilo del chat", 5);
    auto* btnSettings = createNavButton(":/icons/settings.svg", "Configuración", 6);
    auto* btnLogs = createNavButton(":/icons/logs.svg", "Registro / Logs", 7);

    mainLayout->addWidget(btnChat);
    mainLayout->addWidget(btnChannels);
    mainLayout->addWidget(btnWebServer);
    mainLayout->addWidget(btnDonations);
    mainLayout->addWidget(btnStats);
    mainLayout->addWidget(btnStyle);
    mainLayout->addWidget(btnSettings);
    mainLayout->addWidget(btnLogs);

    btnChat->setChecked(true);

    mainLayout->addStretch();

    // Platforms Quick Status Widget
    m_statusFrame = new QFrame(this);
    m_statusFrame->setStyleSheet("background-color: rgba(255,255,255,0.04); border-radius: 8px; padding: 6px;");
    auto* statusLayout = new QVBoxLayout(m_statusFrame);
    statusLayout->setContentsMargins(8, 8, 8, 8);
    statusLayout->setSpacing(4);

    auto* lblPlatformsTitle = new QLabel("ESTADO EN VIVO", m_statusFrame);
    lblPlatformsTitle->setStyleSheet("font-size: 10px; font-weight: bold; color: #9CA3AF; letter-spacing: 0.5px;");
    statusLayout->addWidget(lblPlatformsTitle);

    auto createStatusItem = [this](const QString& platform, const QString& color) -> QLabel* {
        auto* lbl = new QLabel(QString("● %1").arg(platform), this);
        lbl->setStyleSheet(QString("font-size: 11px; color: %1; font-weight: 500;").arg(color));
        return lbl;
    };

    m_lblStatusTwitch = createStatusItem("Twitch", "#6B7280");
    m_lblStatusYouTube = createStatusItem("YouTube", "#6B7280");
    m_lblStatusKick = createStatusItem("Kick", "#6B7280");
    m_lblStatusTikTok = createStatusItem("TikTok", "#6B7280");

    statusLayout->addWidget(m_lblStatusTwitch);
    statusLayout->addWidget(m_lblStatusYouTube);
    statusLayout->addWidget(m_lblStatusKick);
    statusLayout->addWidget(m_lblStatusTikTok);

    mainLayout->addWidget(m_statusFrame);

    // Total Viewers Box
    m_viewersFrame = new QFrame(this);
    m_viewersFrame->setStyleSheet("background-color: #1E293B; border-radius: 8px; padding: 6px;");
    auto* vLayout = new QHBoxLayout(m_viewersFrame);
    vLayout->setContentsMargins(10, 8, 10, 8);

    auto* lblEye = new QLabel(m_viewersFrame);
    lblEye->setPixmap(QIcon(":/icons/eye.svg").pixmap(18, 18));
    
    m_lblTotalViewers = new QLabel("Viewers: --", m_viewersFrame);
    m_lblTotalViewers->setStyleSheet("font-size: 12px; font-weight: bold; color: #38BDF8;");

    vLayout->addWidget(lblEye);
    vLayout->addWidget(m_lblTotalViewers);
    vLayout->addStretch();

    mainLayout->addWidget(m_viewersFrame);

    // Main Connect / Disconnect button
    m_btnConnectAll = new QPushButton("CONECTAR TODOS", this);
    m_btnConnectAll->setProperty("fullText", "CONECTAR TODOS");
    m_btnConnectAll->setIcon(QIcon(":/icons/bolt.svg"));
    m_btnConnectAll->setIconSize(QSize(16, 16));
    m_btnConnectAll->setCursor(Qt::PointingHandCursor);
    m_btnConnectAll->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563EB, stop:1 #3B82F6);"
        "  color: #FFFFFF;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1D4ED8, stop:1 #2563EB);"
        "}"
        "QPushButton:pressed {"
        "  background: #1E40AF;"
        "}"
    );
    connect(m_btnConnectAll, &QPushButton::clicked, this, &Sidebar::onToggleConnectClicked);
    mainLayout->addWidget(m_btnConnectAll);
}

QPushButton* Sidebar::createNavButton(const QString& iconPath, const QString& text, int id)
{
    auto* btn = new QPushButton(text, this);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(18, 18));
    btn->setCheckable(true);
    btn->setProperty("fullText", text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding: 10px 14px;"
        "  border-radius: 8px;"
        "  border: none;"
        "  color: #94A3B8;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  background-color: transparent;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.05);"
        "  color: #F8FAFC;"
        "}"
        "QPushButton:checked {"
        "  background-color: #2563EB;"
        "  color: #FFFFFF;"
        "  font-weight: bold;"
        "}"
    );
    m_buttonGroup->addButton(btn, id);
    m_navButtons.append(btn);
    return btn;
}

void Sidebar::onToggleCollapsed()
{
    m_isCollapsed = !m_isCollapsed;
    applyCollapsedState();
}

void Sidebar::setCollapsed(bool collapsed)
{
    if (m_isCollapsed != collapsed)
    {
        m_isCollapsed = collapsed;
        applyCollapsedState();
    }
}

void Sidebar::applyCollapsedState()
{
    setFixedWidth(m_isCollapsed ? 54 : 220);
    m_brandTitle->setVisible(!m_isCollapsed);
    m_statusFrame->setVisible(!m_isCollapsed);
    m_viewersFrame->setVisible(!m_isCollapsed);
    m_btnCollapse->setIcon(QIcon(m_isCollapsed ? ":/icons/chevron-right.svg" : ":/icons/chevron-left.svg"));
    m_btnCollapse->setToolTip(m_isCollapsed ? "Expandir menú lateral" : "Contraer menú lateral");

    for (QPushButton* button : m_navButtons)
    {
        const QString fullText = button->property("fullText").toString();
        button->setText(m_isCollapsed ? QString() : fullText);
        button->setToolTip(fullText);
        button->setStyleSheet(m_isCollapsed
            ? "QPushButton { text-align: center; padding: 10px 0; border-radius: 8px; border: none; color: #94A3B8; background: transparent; } QPushButton:hover { background: rgba(255,255,255,0.06); color: #F8FAFC; } QPushButton:checked { background: #2563EB; color: #FFFFFF; }"
            : "QPushButton { text-align: left; padding: 9px 12px; border-radius: 8px; border: none; color: #94A3B8; font-size: 13px; font-weight: 500; background-color: transparent; } QPushButton:hover { background-color: rgba(255, 255, 255, 0.05); color: #F8FAFC; } QPushButton:checked { background-color: #2563EB; color: #FFFFFF; font-weight: bold; }");
    }

    m_btnConnectAll->setText(m_isCollapsed ? QString() : m_btnConnectAll->property("fullText").toString());
    m_btnConnectAll->setToolTip(m_btnConnectAll->property("fullText").toString());
}

void Sidebar::setCurrentPage(int index)
{
    QAbstractButton* btn = m_buttonGroup->button(index);
    if (btn)
    {
        btn->setChecked(true);
    }
}

void Sidebar::onButtonClicked(int id)
{
    emit pageSelected(id);
}

void Sidebar::onToggleConnectClicked()
{
    m_isConnectedState = !m_isConnectedState;
    if (m_isConnectedState)
    {
        m_btnConnectAll->setText("DESCONECTAR");
        m_btnConnectAll->setProperty("fullText", "DESCONECTAR");
        m_btnConnectAll->setIcon(QIcon(":/icons/stop.svg"));
        m_btnConnectAll->setStyleSheet(
            "QPushButton {"
            "  background: #DC2626;"
            "  color: #FFFFFF;"
            "  font-weight: bold;"
            "  font-size: 12px;"
            "  border: none;"
            "  border-radius: 8px;"
            "  padding: 10px;"
            "}"
            "QPushButton:hover {"
            "  background: #B91C1C;"
            "}"
        );
        emit connectAllRequested();
    }
    else
    {
        m_btnConnectAll->setText("CONECTAR TODOS");
        m_btnConnectAll->setProperty("fullText", "CONECTAR TODOS");
        m_btnConnectAll->setIcon(QIcon(":/icons/bolt.svg"));
        m_btnConnectAll->setStyleSheet(
            "QPushButton {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563EB, stop:1 #3B82F6);"
            "  color: #FFFFFF;"
            "  font-weight: bold;"
            "  font-size: 12px;"
            "  border: none;"
            "  border-radius: 8px;"
            "  padding: 10px;"
            "}"
            "QPushButton:hover {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1D4ED8, stop:1 #2563EB);"
            "}"
        );
        emit disconnectAllRequested();
    }
    if (m_isCollapsed) applyCollapsedState();
}

void Sidebar::updatePlatformStatus(const QString& platform, PlatformStatus status)
{
    QString color = platformStatusColorHex(status);
    QString statusTxt = platformStatusToString(status);

    QLabel* target = nullptr;
    if (platform.compare("Twitch", Qt::CaseInsensitive) == 0) target = m_lblStatusTwitch;
    else if (platform.compare("YouTube", Qt::CaseInsensitive) == 0) target = m_lblStatusYouTube;
    else if (platform.compare("Kick", Qt::CaseInsensitive) == 0) target = m_lblStatusKick;
    else if (platform.compare("TikTok", Qt::CaseInsensitive) == 0) target = m_lblStatusTikTok;

    if (target)
    {
        target->setText(QString("● %1: %2").arg(platform, statusTxt));
        target->setStyleSheet(QString("font-size: 11px; color: %1; font-weight: 500;").arg(color));
    }
}

void Sidebar::updateTotalViewers(int total)
{
    if (total >= 0)
    {
        m_lblTotalViewers->setText(QString("Viewers: %1").arg(QLocale().toString(total)));
    }
    else
    {
        m_lblTotalViewers->setText("Viewers: --");
    }
}
