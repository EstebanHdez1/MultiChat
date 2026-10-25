#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>

class WebServerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WebServerWidget(QWidget* parent = nullptr);
    ~WebServerWidget() override = default;

public slots:
    void refreshUrls();

private slots:
    void onToggleServerClicked();
    void onOpenLocalUrlClicked();
    void onCopyLocalUrlClicked();
    void onCopyNetworkUrlClicked();
    void onCopyOverlayUrlClicked();
    void onServerStarted(const QString& localUrl, const QString& networkUrl);
    void onServerStopped();
    void onClientCountChanged(int count);

private:
    void setupUi();
    void updateUiState();

    QPushButton* m_btnToggleServer = nullptr;
    QLabel* m_lblStatusBadge = nullptr;
    QLabel* m_lblClientCount = nullptr;

    QLineEdit* m_txtLocalUrl = nullptr;
    QLineEdit* m_txtNetworkUrl = nullptr;
    QLineEdit* m_txtOverlayUrl = nullptr;

    QPushButton* m_btnOpenLocal = nullptr;
    QPushButton* m_btnCopyLocal = nullptr;
    QPushButton* m_btnCopyNetwork = nullptr;
    QPushButton* m_btnCopyOverlay = nullptr;

    QSpinBox* m_spinHttpPort = nullptr;
    QSpinBox* m_spinWsPort = nullptr;
};
