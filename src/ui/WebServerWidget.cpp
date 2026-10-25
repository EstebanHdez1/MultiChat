#include "WebServerWidget.h"
#include "LocalWebServer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QTimer>

WebServerWidget::WebServerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    connect(&LocalWebServer::instance(), &LocalWebServer::serverStarted, this, &WebServerWidget::onServerStarted);
    connect(&LocalWebServer::instance(), &LocalWebServer::serverStopped, this, &WebServerWidget::onServerStopped);
    connect(&LocalWebServer::instance(), &LocalWebServer::clientCountChanged, this, &WebServerWidget::onClientCountChanged);

    updateUiState();
}

void WebServerWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(20);

    // Header
    auto* headerLayout = new QHBoxLayout();
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/icons/globe.svg").pixmap(24, 24));

    auto* titleContainer = new QVBoxLayout();
    titleContainer->setSpacing(4);
    auto* lblTitle = new QLabel("SERVIDOR WEB Y MODO MÓVIL / OBS", this);
    lblTitle->setStyleSheet("font-size: 18px; font-weight: 700; color: #FFFFFF; letter-spacing: -0.3px;");
    auto* lblSub = new QLabel("Accede al chat unificado en tiempo real desde tu navegador, tu celular (vía Wi-Fi) o como Overlay para OBS Studio.", this);
    lblSub->setStyleSheet("font-size: 13px; color: #94A3B8;");
    titleContainer->addWidget(lblTitle);
    titleContainer->addWidget(lblSub);

    headerLayout->addWidget(iconLabel);
    headerLayout->addLayout(titleContainer);
    headerLayout->addStretch();

    mainLayout->addLayout(headerLayout);

    // Card 1: Server Controls
    auto* cardControl = new QFrame(this);
    cardControl->setStyleSheet("background-color: #161B26; border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; padding: 16px;");
    auto* controlLayout = new QVBoxLayout(cardControl);
    controlLayout->setSpacing(14);

    auto* topRow = new QHBoxLayout();
    m_lblStatusBadge = new QLabel("🔴 SERVIDOR DETENIDO", cardControl);
    m_lblStatusBadge->setStyleSheet("background: rgba(239, 68, 68, 0.15); color: #F87171; font-weight: 700; font-size: 12px; padding: 5px 12px; border-radius: 9999px; border: 1px solid rgba(239, 68, 68, 0.3);");

    m_lblClientCount = new QLabel("0 clientes conectados", cardControl);
    m_lblClientCount->setStyleSheet("color: #94A3B8; font-size: 12px;");

    topRow->addWidget(m_lblStatusBadge);
    topRow->addWidget(m_lblClientCount);
    topRow->addStretch();

    // Port settings
    auto* portRow = new QHBoxLayout();
    portRow->setSpacing(12);

    auto* lblHttp = new QLabel("Puerto Web (HTTP):", cardControl);
    lblHttp->setStyleSheet("color: #CBD5E1; font-size: 13px; font-weight: 600;");
    m_spinHttpPort = new QSpinBox(cardControl);
    m_spinHttpPort->setRange(1024, 65535);
    m_spinHttpPort->setValue(8080);
    m_spinHttpPort->setFixedWidth(90);
    m_spinHttpPort->setStyleSheet("background: #0F131C; border: 1px solid #334155; border-radius: 6px; padding: 4px 8px; color: #FFF; font-weight: bold;");

    auto* lblWs = new QLabel("Puerto WebSocket:", cardControl);
    lblWs->setStyleSheet("color: #CBD5E1; font-size: 13px; font-weight: 600; margin-left: 12px;");
    m_spinWsPort = new QSpinBox(cardControl);
    m_spinWsPort->setRange(1024, 65535);
    m_spinWsPort->setValue(8081);
    m_spinWsPort->setFixedWidth(90);
    m_spinWsPort->setStyleSheet("background: #0F131C; border: 1px solid #334155; border-radius: 6px; padding: 4px 8px; color: #FFF; font-weight: bold;");

    m_btnToggleServer = new QPushButton("INICIAR SERVIDOR WEB", cardControl);
    m_btnToggleServer->setCursor(Qt::PointingHandCursor);
    m_btnToggleServer->setStyleSheet("QPushButton { background: #2563EB; color: #FFF; font-weight: 700; font-size: 13px; padding: 8px 18px; border-radius: 8px; border: none; } QPushButton:hover { background: #1D4ED8; }");
    connect(m_btnToggleServer, &QPushButton::clicked, this, &WebServerWidget::onToggleServerClicked);

    portRow->addWidget(lblHttp);
    portRow->addWidget(m_spinHttpPort);
    portRow->addWidget(lblWs);
    portRow->addWidget(m_spinWsPort);
    portRow->addStretch();
    portRow->addWidget(m_btnToggleServer);

    controlLayout->addLayout(topRow);
    controlLayout->addLayout(portRow);
    mainLayout->addWidget(cardControl);

    // Helper for URL entry cards
    auto createUrlCard = [this](const QString& title, const QString& desc, QLineEdit*& txtOut, QPushButton*& btnCopyOut, QPushButton* btnOpen = nullptr) -> QFrame* {
        auto* card = new QFrame(this);
        card->setStyleSheet("background-color: #121622; border: 1px solid rgba(255,255,255,0.06); border-radius: 12px; padding: 14px 18px;");
        auto* layout = new QVBoxLayout(card);
        layout->setSpacing(8);

        auto* lTitle = new QLabel(title, card);
        lTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #F1F5F9;");
        auto* lDesc = new QLabel(desc, card);
        lDesc->setStyleSheet("font-size: 12px; color: #94A3B8;");

        auto* inputRow = new QHBoxLayout();
        inputRow->setSpacing(8);

        txtOut = new QLineEdit(card);
        txtOut->setReadOnly(true);
        txtOut->setStyleSheet("background: #0B0E14; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px 12px; color: #38BDF8; font-family: monospace; font-size: 13px; font-weight: 600;");

        btnCopyOut = new QPushButton("Copiar", card);
        btnCopyOut->setCursor(Qt::PointingHandCursor);
        btnCopyOut->setStyleSheet("QPushButton { background: #334155; color: #FFF; font-size: 12px; font-weight: 600; padding: 8px 14px; border-radius: 8px; border: none; } QPushButton:hover { background: #475569; }");

        inputRow->addWidget(txtOut, 1);
        inputRow->addWidget(btnCopyOut);

        if (btnOpen)
        {
            inputRow->addWidget(btnOpen);
        }

        layout->addWidget(lTitle);
        layout->addWidget(lDesc);
        layout->addLayout(inputRow);
        return card;
    };

    // Card 2: Local PC Access
    m_btnOpenLocal = new QPushButton("Abrir en Navegador", this);
    m_btnOpenLocal->setCursor(Qt::PointingHandCursor);
    m_btnOpenLocal->setStyleSheet("QPushButton { background: #4F46E5; color: #FFF; font-size: 12px; font-weight: 600; padding: 8px 14px; border-radius: 8px; border: none; } QPushButton:hover { background: #4338CA; }");
    connect(m_btnOpenLocal, &QPushButton::clicked, this, &WebServerWidget::onOpenLocalUrlClicked);

    auto* cardLocal = createUrlCard(
        "💻 Acceso Local (Este PC)",
        "Abre el chat unificado directamente en una pestaña de tu navegador (Chrome, Edge, Firefox, Brave).",
        m_txtLocalUrl,
        m_btnCopyLocal,
        m_btnOpenLocal
    );
    connect(m_btnCopyLocal, &QPushButton::clicked, this, &WebServerWidget::onCopyLocalUrlClicked);
    mainLayout->addWidget(cardLocal);

    // Card 3: Mobile Network Access
    auto* cardNetwork = createUrlCard(
        "📱 Acceso Móvil / Red Wi-Fi",
        "Abre esta dirección en el navegador de tu teléfono móvil o tablet (conectado a la misma red Wi-Fi).",
        m_txtNetworkUrl,
        m_btnCopyNetwork
    );
    connect(m_btnCopyNetwork, &QPushButton::clicked, this, &WebServerWidget::onCopyNetworkUrlClicked);
    mainLayout->addWidget(cardNetwork);

    // Card 4: OBS Overlay
    auto* cardOverlay = createUrlCard(
        "🎬 Overlay Transparente para OBS Studio",
        "Añade una fuente de Navegador en OBS Studio con este enlace y fondo transparente para mostrar el chat en tu directo.",
        m_txtOverlayUrl,
        m_btnCopyOverlay
    );
    connect(m_btnCopyOverlay, &QPushButton::clicked, this, &WebServerWidget::onCopyOverlayUrlClicked);
    mainLayout->addWidget(cardOverlay);

    mainLayout->addStretch();
}

void WebServerWidget::updateUiState()
{
    bool running = LocalWebServer::instance().isRunning();

    if (running)
    {
        m_lblStatusBadge->setText(QString("🟢 ACTIVO EN PUERTO %1").arg(LocalWebServer::instance().httpPort()));
        m_lblStatusBadge->setStyleSheet("background: rgba(16, 185, 129, 0.15); color: #34D399; font-weight: 700; font-size: 12px; padding: 5px 12px; border-radius: 9999px; border: 1px solid rgba(16, 185, 129, 0.3);");
        m_btnToggleServer->setText("DETENER SERVIDOR");
        m_btnToggleServer->setStyleSheet("QPushButton { background: #DC2626; color: #FFF; font-weight: 700; font-size: 13px; padding: 8px 18px; border-radius: 8px; border: none; } QPushButton:hover { background: #B91C1C; }");
        m_spinHttpPort->setEnabled(false);
        m_spinWsPort->setEnabled(false);
    }
    else
    {
        m_lblStatusBadge->setText("🔴 SERVIDOR DETENIDO");
        m_lblStatusBadge->setStyleSheet("background: rgba(239, 68, 68, 0.15); color: #F87171; font-weight: 700; font-size: 12px; padding: 5px 12px; border-radius: 9999px; border: 1px solid rgba(239, 68, 68, 0.3);");
        m_btnToggleServer->setText("INICIAR SERVIDOR WEB");
        m_btnToggleServer->setStyleSheet("QPushButton { background: #2563EB; color: #FFF; font-weight: 700; font-size: 13px; padding: 8px 18px; border-radius: 8px; border: none; } QPushButton:hover { background: #1D4ED8; }");
        m_spinHttpPort->setEnabled(true);
        m_spinWsPort->setEnabled(true);
    }

    refreshUrls();
}

void WebServerWidget::refreshUrls()
{
    quint16 port = LocalWebServer::instance().isRunning() ? LocalWebServer::instance().httpPort() : m_spinHttpPort->value();
    QString ip = LocalWebServer::instance().getLocalIpAddress();

    m_txtLocalUrl->setText(QString("http://localhost:%1").arg(port));
    m_txtNetworkUrl->setText(QString("http://%1:%2").arg(ip).arg(port));
    m_txtOverlayUrl->setText(QString("http://localhost:%1/?overlay=true").arg(port));
}

void WebServerWidget::onToggleServerClicked()
{
    if (LocalWebServer::instance().isRunning())
    {
        LocalWebServer::instance().stopServer();
    }
    else
    {
        LocalWebServer::instance().startServer(m_spinHttpPort->value(), m_spinWsPort->value());
    }
    updateUiState();
}

void WebServerWidget::onOpenLocalUrlClicked()
{
    if (!LocalWebServer::instance().isRunning())
    {
        LocalWebServer::instance().startServer(m_spinHttpPort->value(), m_spinWsPort->value());
        updateUiState();
    }
    QDesktopServices::openUrl(QUrl(m_txtLocalUrl->text()));
}

void WebServerWidget::onCopyLocalUrlClicked()
{
    QGuiApplication::clipboard()->setText(m_txtLocalUrl->text());
    m_btnCopyLocal->setText("¡Copiado!");
    QTimer::singleShot(1500, this, [this]() { m_btnCopyLocal->setText("Copiar"); });
}

void WebServerWidget::onCopyNetworkUrlClicked()
{
    QGuiApplication::clipboard()->setText(m_txtNetworkUrl->text());
    m_btnCopyNetwork->setText("¡Copiado!");
    QTimer::singleShot(1500, this, [this]() { m_btnCopyNetwork->setText("Copiar"); });
}

void WebServerWidget::onCopyOverlayUrlClicked()
{
    QGuiApplication::clipboard()->setText(m_txtOverlayUrl->text());
    m_btnCopyOverlay->setText("¡Copiado!");
    QTimer::singleShot(1500, this, [this]() { m_btnCopyOverlay->setText("Copiar"); });
}

void WebServerWidget::onServerStarted(const QString& localUrl, const QString& networkUrl)
{
    Q_UNUSED(localUrl);
    Q_UNUSED(networkUrl);
    updateUiState();
}

void WebServerWidget::onServerStopped()
{
    updateUiState();
}

void WebServerWidget::onClientCountChanged(int count)
{
    m_lblClientCount->setText(QString("%1 cliente%2 conectado%3")
        .arg(count).arg(count == 1 ? "" : "s").arg(count == 1 ? "" : "s"));
}
