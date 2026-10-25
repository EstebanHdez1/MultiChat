#include "MainWindow.h"
#include "ConfigManager.h"
#include "ConnectionManager.h"
#include "EventManager.h"
#include <QHBoxLayout>
#include <QIcon>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_sidebar(new Sidebar(this))
    , m_stackedWidget(new QStackedWidget(this))
    , m_chatWidget(new ChatWidget(this))
    , m_channelWidget(new ChannelManagerWidget(this))
    , m_webServerWidget(new WebServerWidget(this))
    , m_donationsWidget(new DonationsWidget(this))
    , m_statsWidget(new StatisticsWidget(this))
    , m_styleWidget(new StyleSettingsWidget(this))
    , m_settingsWidget(new GeneralSettingsWidget(this))
    , m_logsWidget(new LogViewerWidget(this))
{
    setupUi();

    // Wire up sidebar navigation
    connect(m_sidebar, &Sidebar::pageSelected, this, &MainWindow::onPageSelected);
    connect(m_sidebar, &Sidebar::connectAllRequested, this, &MainWindow::onConnectAll);
    connect(m_sidebar, &Sidebar::disconnectAllRequested, this, &MainWindow::onDisconnectAll);

    // Wire up ConfigManager style changes
    connect(&ConfigManager::instance(), &ConfigManager::styleChanged, this, &MainWindow::onStyleChanged);

    // Wire up EventManager status & viewers
    connect(&EventManager::instance(), &EventManager::platformStatusChanged, this, &MainWindow::onPlatformStatusChanged);
    connect(&EventManager::instance(), &EventManager::viewerCountUpdated, this, &MainWindow::onViewerCountUpdated);

    // Apply initial style
    applyTheme(ConfigManager::instance().getStyleConfig());
}

void MainWindow::setupUi()
{
    setWindowTitle("MultiChat - Unified Live Stream Chat");
    resize(1180, 760);
    setMinimumSize(880, 560);

    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Add Sidebar
    mainLayout->addWidget(m_sidebar);

    // Add Stacked Pages
    m_stackedWidget->addWidget(m_chatWidget);       // 0
    m_stackedWidget->addWidget(m_channelWidget);    // 1
    m_stackedWidget->addWidget(m_webServerWidget);  // 2
    m_stackedWidget->addWidget(m_donationsWidget);  // 3
    m_stackedWidget->addWidget(m_statsWidget);      // 4
    m_stackedWidget->addWidget(m_styleWidget);      // 5
    m_stackedWidget->addWidget(m_settingsWidget);   // 6
    m_stackedWidget->addWidget(m_logsWidget);       // 7

    mainLayout->addWidget(m_stackedWidget, 1);
}

void MainWindow::onPageSelected(int index)
{
    m_stackedWidget->setCurrentIndex(index);
}

void MainWindow::onConnectAll()
{
    ConnectionManager::instance().connectAll();
}

void MainWindow::onDisconnectAll()
{
    ConnectionManager::instance().disconnectAll();
}

void MainWindow::onStyleChanged(const AppStyleConfig& style)
{
    applyTheme(style);
    m_chatWidget->applyStyle(style);
}

void MainWindow::applyTheme(const AppStyleConfig& style)
{
    QString bg = style.backgroundColor;
    // Keep the primary application window visible even when a previous OBS
    // preset was persisted. A transparent native window can otherwise appear
    // as though it never opened on Windows window capture/compositors.
    if (bg == "transparent") bg = "#12141A";

    setStyleSheet(QString(
        "QMainWindow, QWidget#SidebarWidget, QWidget#qt_scrollarea_viewport {"
        "  background-color: %1;"
        "}"
        "QScrollBar:vertical {"
        "  background: #111827;"
        "  width: 8px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #374151;"
        "  min-height: 20px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #4B5563;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    ).arg(bg));
}

void MainWindow::onPlatformStatusChanged(const QString& platform, PlatformStatus status, const QString& detail)
{
    Q_UNUSED(detail);
    m_sidebar->updatePlatformStatus(platform, status);
}

void MainWindow::onViewerCountUpdated(const QString& platform, int count, int total)
{
    Q_UNUSED(platform);
    Q_UNUSED(count);
    m_sidebar->updateTotalViewers(total);
}
