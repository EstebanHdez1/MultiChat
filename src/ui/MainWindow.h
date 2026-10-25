#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include "Sidebar.h"
#include "ChatWidget.h"
#include "ChannelManagerWidget.h"
#include "WebServerWidget.h"
#include "DonationsWidget.h"
#include "StatisticsWidget.h"
#include "StyleSettingsWidget.h"
#include "GeneralSettingsWidget.h"
#include "LogViewerWidget.h"
#include "AppStyleConfig.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onPageSelected(int index);
    void onConnectAll();
    void onDisconnectAll();
    void onStyleChanged(const AppStyleConfig& style);
    void onPlatformStatusChanged(const QString& platform, PlatformStatus status, const QString& detail);
    void onViewerCountUpdated(const QString& platform, int count, int total);

private:
    void setupUi();
    void applyTheme(const AppStyleConfig& style);

    Sidebar* m_sidebar;
    QStackedWidget* m_stackedWidget;

    ChatWidget* m_chatWidget;
    ChannelManagerWidget* m_channelWidget;
    WebServerWidget* m_webServerWidget;
    DonationsWidget* m_donationsWidget;
    StatisticsWidget* m_statsWidget;
    StyleSettingsWidget* m_styleWidget;
    GeneralSettingsWidget* m_settingsWidget;
    LogViewerWidget* m_logsWidget;
};
