#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "DonationManager.h"

class DonationsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DonationsWidget(QWidget* parent = nullptr);
    ~DonationsWidget() override = default;

    void refreshDonations();

private slots:
    void onSortChanged(int index);
    void onClearDonationsClicked();

private:
    void setupUi();

    QTableWidget* m_table;
    QComboBox* m_cmbSort;
    QPushButton* m_btnClear;
    QLabel* m_lblTotalCount;
    QTimer m_refreshTimer;
};
