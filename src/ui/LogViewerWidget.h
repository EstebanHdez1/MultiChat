#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include "Logger.h"

class LogViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogViewerWidget(QWidget* parent = nullptr);
    ~LogViewerWidget() override = default;

    void appendLog(const LogEntry& entry);
    void refreshLogs();

private slots:
    void onClearClicked();
    void onCopyClicked();
    void onFilterChanged();

private:
    void setupUi();

    QPlainTextEdit* m_txtLogs;
    QLineEdit* m_edtSearch;
    QComboBox* m_cmbLevel;
    QPushButton* m_btnClear;
    QPushButton* m_btnCopy;
};
