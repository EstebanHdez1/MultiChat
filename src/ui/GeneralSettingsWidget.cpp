#include "GeneralSettingsWidget.h"
#include "ConfigManager.h"
#include "ChatManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QIcon>

GeneralSettingsWidget::GeneralSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    loadSettings();
}

void GeneralSettingsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    auto* titleLayout = new QHBoxLayout();
    auto* titleIcon = new QLabel(this);
    titleIcon->setPixmap(QIcon(":/icons/settings.svg").pixmap(20, 20));
    auto* titleLabel = new QLabel("CONFIGURACIÓN GENERAL DEL SISTEMA", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;");
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    mainLayout->addLayout(titleLayout);

    // Section 1: Transient Live Notifications & Likes Anti-Spam
    auto* eventsFrame = new QFrame(this);
    eventsFrame->setObjectName("eventsFrame");
    eventsFrame->setStyleSheet(
        "QFrame#eventsFrame {"
        "  background-color: #1A1D24;"
        "  border-radius: 10px;"
        "  border: 1px solid #2D3748;"
        "}"
    );
    auto* eventsLayout = new QVBoxLayout(eventsFrame);
    eventsLayout->setContentsMargins(16, 16, 16, 16);
    eventsLayout->setSpacing(12);

    auto* lblEventsTitle = new QLabel("NOTIFICACIONES TEMPORALES Y ANTI-SPAM DE LIKES", eventsFrame);
    lblEventsTitle->setStyleSheet("font-size: 12px; font-weight: bold; color: #60A5FA; background: transparent; border: none;");
    eventsLayout->addWidget(lblEventsTitle);

    // Duration Setting
    auto* durLayout = new QHBoxLayout();
    auto* lblDur = new QLabel("Tiempo que dura en desaparecer la notificación flotante:", eventsFrame);
    lblDur->setStyleSheet("color: #D1D5DB; font-size: 12px; background: transparent; border: none;");
    m_spnTransientDuration = new QDoubleSpinBox(eventsFrame);
    m_spnTransientDuration->setRange(1.0, 30.0);
    m_spnTransientDuration->setSingleStep(0.5);
    m_spnTransientDuration->setSuffix(" seg");
    m_spnTransientDuration->setValue(4.5);
    m_spnTransientDuration->setStyleSheet(
        "QDoubleSpinBox {"
        "  background-color: #111827;"
        "  color: #FFFFFF;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  min-width: 90px;"
        "}"
    );
    durLayout->addWidget(lblDur);
    durLayout->addWidget(m_spnTransientDuration);
    durLayout->addStretch();
    eventsLayout->addLayout(durLayout);

    // Likes Mode Setting
    auto* likeModeLayout = new QHBoxLayout();
    auto* lblLikeMode = new QLabel("Formato de notificación de Likes:", eventsFrame);
    lblLikeMode->setStyleSheet("color: #D1D5DB; font-size: 12px; background: transparent; border: none;");
    m_cmbLikeMode = new QComboBox(eventsFrame);
    m_cmbLikeMode->addItem("Solo indicar quién dio like (Anti-spam recomendado)", false);
    m_cmbLikeMode->addItem("Mostrar cantidad enviada (ej: ¡ha enviado 8 likes!)", true);
    m_cmbLikeMode->setStyleSheet(
        "QComboBox {"
        "  background-color: #111827;"
        "  color: #FFFFFF;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  font-size: 12px;"
        "}"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #1A1D24;"
        "  color: #FFFFFF;"
        "  selection-background-color: #2563EB;"
        "  border: 1px solid #374151;"
        "}"
    );
    likeModeLayout->addWidget(lblLikeMode);
    likeModeLayout->addWidget(m_cmbLikeMode);
    likeModeLayout->addStretch();
    eventsLayout->addLayout(likeModeLayout);

    // Likes Cooldown Setting
    auto* coolLayout = new QHBoxLayout();
    auto* lblCool = new QLabel("Tiempo de espera anti-spam entre likes del mismo usuario:", eventsFrame);
    lblCool->setStyleSheet("color: #D1D5DB; font-size: 12px; background: transparent; border: none;");
    m_spnLikeCooldown = new QSpinBox(eventsFrame);
    m_spnLikeCooldown->setRange(1, 60);
    m_spnLikeCooldown->setSingleStep(1);
    m_spnLikeCooldown->setSuffix(" seg");
    m_spnLikeCooldown->setValue(10);
    m_spnLikeCooldown->setStyleSheet(
        "QSpinBox {"
        "  background-color: #111827;"
        "  color: #FFFFFF;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  min-width: 90px;"
        "}"
    );
    coolLayout->addWidget(lblCool);
    coolLayout->addWidget(m_spnLikeCooldown);
    coolLayout->addStretch();
    eventsLayout->addLayout(coolLayout);

    // Checkboxes for individual transient event types
    auto* checkGrid = new QHBoxLayout();
    m_chkShowJoins = new QCheckBox("Uniones", eventsFrame);
    m_chkShowJoins->setIcon(QIcon(":/icons/user.svg"));
    m_chkShowShares = new QCheckBox("Compartidos", eventsFrame);
    m_chkShowShares->setIcon(QIcon(":/icons/share.svg"));
    m_chkShowFollows = new QCheckBox("Seguidores", eventsFrame);
    m_chkShowFollows->setIcon(QIcon(":/icons/star.svg"));
    m_chkShowLikes = new QCheckBox("Likes", eventsFrame);
    m_chkShowLikes->setIcon(QIcon(":/icons/heart.svg"));

    auto styleChk = [](QCheckBox* c) {
        c->setChecked(true);
        c->setStyleSheet("QCheckBox { color: #E5E7EB; font-size: 12px; background: transparent; border: none; spacing: 6px; }");
    };
    styleChk(m_chkShowJoins);
    styleChk(m_chkShowShares);
    styleChk(m_chkShowFollows);
    styleChk(m_chkShowLikes);

    checkGrid->addWidget(m_chkShowJoins);
    checkGrid->addWidget(m_chkShowShares);
    checkGrid->addWidget(m_chkShowFollows);
    checkGrid->addWidget(m_chkShowLikes);
    checkGrid->addStretch();
    eventsLayout->addLayout(checkGrid);

    mainLayout->addWidget(eventsFrame);

    // Section 2: Performance & Buffer Frame
    auto* perfFrame = new QFrame(this);
    perfFrame->setObjectName("perfFrame");
    perfFrame->setStyleSheet(
        "QFrame#perfFrame {"
        "  background-color: #1A1D24;"
        "  border-radius: 10px;"
        "  border: 1px solid #2D3748;"
        "}"
    );
    auto* perfLayout = new QVBoxLayout(perfFrame);
    perfLayout->setContentsMargins(16, 16, 16, 16);
    perfLayout->setSpacing(12);

    auto* lblPerfTitle = new QLabel("RENDIMIENTO Y BUFFER DE MENSAJES", perfFrame);
    lblPerfTitle->setStyleSheet("font-size: 12px; font-weight: bold; color: #9CA3AF; background: transparent; border: none;");
    perfLayout->addWidget(lblPerfTitle);

    auto* msgLayout = new QHBoxLayout();
    auto* lblMaxMsg = new QLabel("Límite de mensajes en memoria (Circular Buffer):", perfFrame);
    lblMaxMsg->setStyleSheet("color: #D1D5DB; font-size: 12px; background: transparent; border: none;");
    m_spnMaxMessages = new QSpinBox(perfFrame);
    m_spnMaxMessages->setRange(50, 5000);
    m_spnMaxMessages->setSingleStep(50);
    m_spnMaxMessages->setValue(500);
    m_spnMaxMessages->setStyleSheet(
        "QSpinBox {"
        "  background-color: #111827;"
        "  color: #FFFFFF;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  min-width: 90px;"
        "}"
    );
    
    msgLayout->addWidget(lblMaxMsg);
    msgLayout->addWidget(m_spnMaxMessages);
    msgLayout->addStretch();
    perfLayout->addLayout(msgLayout);

    m_chkAutoScroll = new QCheckBox("Habilitar Auto-scroll por defecto al iniciar", perfFrame);
    m_chkAutoScroll->setChecked(true);
    m_chkAutoScroll->setStyleSheet("QCheckBox { color: #D1D5DB; font-size: 12px; background: transparent; border: none; spacing: 6px; }");
    perfLayout->addWidget(m_chkAutoScroll);

    m_chkSounds = new QCheckBox("Reproducir sonido en eventos de donaciones", perfFrame);
    m_chkSounds->setChecked(false);
    m_chkSounds->setStyleSheet("QCheckBox { color: #D1D5DB; font-size: 12px; background: transparent; border: none; spacing: 6px; }");
    perfLayout->addWidget(m_chkSounds);

    mainLayout->addWidget(perfFrame);

    // Section 3: Connection & Security Information
    auto* infoFrame = new QFrame(this);
    infoFrame->setObjectName("infoFrame");
    infoFrame->setStyleSheet(
        "QFrame#infoFrame {"
        "  background-color: #1A1D24;"
        "  border-radius: 10px;"
        "  border: 1px solid #2D3748;"
        "}"
    );
    auto* infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(16, 16, 16, 16);
    infoLayout->setSpacing(8);

    auto* lblInfoTitle = new QLabel("INFORMACIÓN DE CONEXIÓN Y PRIVACIDAD", infoFrame);
    lblInfoTitle->setStyleSheet("font-size: 12px; font-weight: bold; color: #9CA3AF; background: transparent; border: none;");
    infoLayout->addWidget(lblInfoTitle);

    auto* lblDesc = new QLabel(
        "• <b>Twitch</b>: Conexión IRC WebSocket segura (anónimo sin login).<br>"
        "• <b>YouTube</b>: Live streaming handshake y consulta pública a InnerTube.<br>"
        "• <b>Kick</b>: Handshake de canal y suscripción a WebSockets Pusher en vivo.<br>"
        "• <b>TikTok</b>: Extracción pública de Webcast y eventos de sala en tiempo real.<br><br>"
        "<i>Ninguna de tus credenciales personales es requerida ni almacenada por esta aplicación.</i>", 
        infoFrame);
    lblDesc->setStyleSheet("color: #9CA3AF; font-size: 12px; line-height: 1.4; background: transparent; border: none;");
    lblDesc->setWordWrap(true);
    infoLayout->addWidget(lblDesc);

    mainLayout->addWidget(infoFrame);

    // Action Buttons
    auto* btnLayout = new QHBoxLayout();
    m_btnSave = new QPushButton("Guardar Configuración", this);
    m_btnSave->setIcon(QIcon(":/icons/save.svg"));
    m_btnSave->setIconSize(QSize(16, 16));
    m_btnSave->setCursor(Qt::PointingHandCursor);
    m_btnSave->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; border-radius: 6px; padding: 8px 16px; } QPushButton:hover { background: #1D4ED8; }");
    connect(m_btnSave, &QPushButton::clicked, this, &GeneralSettingsWidget::onSaveClicked);

    m_btnReset = new QPushButton("Restaurar por Defecto", this);
    m_btnReset->setIcon(QIcon(":/icons/refresh.svg"));
    m_btnReset->setIconSize(QSize(14, 14));
    m_btnReset->setCursor(Qt::PointingHandCursor);
    m_btnReset->setStyleSheet("QPushButton { background: #374151; color: white; border-radius: 6px; padding: 8px 16px; } QPushButton:hover { background: #4B5563; }");
    connect(m_btnReset, &QPushButton::clicked, this, &GeneralSettingsWidget::onResetDefaultsClicked);

    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnReset);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();
}

void GeneralSettingsWidget::loadSettings()
{
    AppStyleConfig style = ConfigManager::instance().getStyleConfig();
    m_spnMaxMessages->setValue(style.maxVisibleMessages);
    m_chkAutoScroll->setChecked(style.autoScroll);
    m_chkSounds->setChecked(style.soundNotifications);

    m_spnTransientDuration->setValue(style.transientDurationSec > 0.0 ? style.transientDurationSec : 4.5);
    int likeIndex = m_cmbLikeMode->findData(style.showLikeCount);
    if (likeIndex >= 0) m_cmbLikeMode->setCurrentIndex(likeIndex);
    m_spnLikeCooldown->setValue(style.likeCooldownSec > 0 ? style.likeCooldownSec : 10);
    m_chkShowJoins->setChecked(style.showJoinEvents);
    m_chkShowShares->setChecked(style.showShareEvents);
    m_chkShowFollows->setChecked(style.showFollowEvents);
    m_chkShowLikes->setChecked(style.showLikeEvents);
}

void GeneralSettingsWidget::onSaveClicked()
{
    AppStyleConfig style = ConfigManager::instance().getStyleConfig();
    style.maxVisibleMessages = m_spnMaxMessages->value();
    style.autoScroll = m_chkAutoScroll->isChecked();
    style.soundNotifications = m_chkSounds->isChecked();

    style.transientDurationSec = m_spnTransientDuration->value();
    style.showLikeCount = m_cmbLikeMode->currentData().toBool();
    style.likeCooldownSec = m_spnLikeCooldown->value();
    style.showJoinEvents = m_chkShowJoins->isChecked();
    style.showShareEvents = m_chkShowShares->isChecked();
    style.showFollowEvents = m_chkShowFollows->isChecked();
    style.showLikeEvents = m_chkShowLikes->isChecked();

    ChatManager::instance().setMaxMessages(style.maxVisibleMessages);
    ConfigManager::instance().setStyleConfig(style);
    Logger::instance().info("Settings", "Configuración general y de notificaciones guardada correctamente.");
}

void GeneralSettingsWidget::onResetDefaultsClicked()
{
    m_spnMaxMessages->setValue(500);
    m_chkAutoScroll->setChecked(true);
    m_chkSounds->setChecked(false);

    m_spnTransientDuration->setValue(4.5);
    m_cmbLikeMode->setCurrentIndex(0); // Solo indicar quién dio like
    m_spnLikeCooldown->setValue(10);
    m_chkShowJoins->setChecked(true);
    m_chkShowShares->setChecked(true);
    m_chkShowFollows->setChecked(true);
    m_chkShowLikes->setChecked(true);

    onSaveClicked();
}
