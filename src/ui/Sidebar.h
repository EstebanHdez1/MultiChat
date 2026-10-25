#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QList>
#include "PlatformStatus.h"

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() override = default;

    void setCurrentPage(int index);
    void updatePlatformStatus(const QString& platform, PlatformStatus status);
    void updateTotalViewers(int total);

signals:
    void pageSelected(int index);
    void connectAllRequested();
    void disconnectAllRequested();

private slots:
    void onButtonClicked(int id);
    void onToggleConnectClicked();
    void onToggleCollapsed();

private:
    void setupUi();
    QPushButton* createNavButton(const QString& icon, const QString& text, int id);
    void applyCollapsedState();

    QButtonGroup* m_buttonGroup;
    QPushButton* m_btnConnectAll;
    QPushButton* m_btnCollapse;
    QList<QPushButton*> m_navButtons;
    QLabel* m_brandTitle;
    QFrame* m_statusFrame;
    QFrame* m_viewersFrame;
    QLabel* m_lblTotalViewers;
    QLabel* m_lblStatusTwitch;
    QLabel* m_lblStatusYouTube;
    QLabel* m_lblStatusKick;
    QLabel* m_lblStatusTikTok;

    bool m_isConnectedState = false;
    bool m_isCollapsed = false;
};
