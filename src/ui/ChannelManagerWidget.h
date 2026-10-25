#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include "ChannelConfig.h"
#include "PlatformStatus.h"

struct PlatformCardWidgets
{
    QString platform;
    QLineEdit* edtChannel;
    QLineEdit* edtExtra;
    QLabel* lblStatus;
    QLabel* lblViewers;
    QPushButton* btnToggleConnect;
    QCheckBox* chkEnabled;
};

class ChannelManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChannelManagerWidget(QWidget* parent = nullptr);
    ~ChannelManagerWidget() override = default;

    void updateStatus(const QString& platform, PlatformStatus status, const QString& detail);
    void updateViewers(const QString& platform, int count);
    void loadFromConfig();

private slots:
    void onSaveAllClicked();
    void onTogglePlatformClicked(const QString& platform);

private:
    void setupUi();
    QWidget* createPlatformCard(const QString& platform, const QString& color, const QString& iconPath, const QString& placeholder, const QString& extraLabel = "");

    QMap<QString, PlatformCardWidgets> m_cards;
    QPushButton* m_btnSaveAll;
};
