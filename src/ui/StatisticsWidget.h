#pragma once

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QFrame>

class StatisticsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatisticsWidget(QWidget* parent = nullptr);
    ~StatisticsWidget() override = default;

    void updateViewerCounts();

private:
    void setupUi();
    QWidget* createStatCard(const QString& title, const QString& icon, const QString& color, QLabel*& outValueLabel, QLabel*& outStatusLabel);

    QLabel* m_lblTwitchViewers;
    QLabel* m_lblTwitchStatus;

    QLabel* m_lblYouTubeViewers;
    QLabel* m_lblYouTubeStatus;

    QLabel* m_lblKickViewers;
    QLabel* m_lblKickStatus;

    QLabel* m_lblTikTokViewers;
    QLabel* m_lblTikTokStatus;

    QLabel* m_lblTotalViewers;
    QLabel* m_lblConnectedCount;
};
