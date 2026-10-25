#include <QApplication>
#include <QIcon>
#include "MainWindow.h"
#include "ConfigManager.h"
#include "ConnectionManager.h"
#include "ChatManager.h"
#include "Logger.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MultistreamChat");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Multistream");

    Logger::instance().info("Main", "Initializing Multistream Chat v1.0.0...");

    // Load persisted configuration
    ConfigManager::instance().loadConfig();
    ChatManager::instance().setMaxMessages(ConfigManager::instance().getStyleConfig().maxVisibleMessages);

    // Initialize platform connection manager
    ConnectionManager::instance().initialize();

    // Create & Show GUI
    MainWindow mainWindow;
    mainWindow.show();

    Logger::instance().info("Main", "Application UI started successfully.");

    return app.exec();
}
