#include "LogViewerWidget.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QIcon>

LogViewerWidget::LogViewerWidget(QWidget* parent)
    : QWidget(parent)
    , m_txtLogs(new QPlainTextEdit(this))
    , m_edtSearch(new QLineEdit(this))
    , m_cmbLevel(new QComboBox(this))
    , m_btnClear(new QPushButton("Limpiar", this))
    , m_btnCopy(new QPushButton("Copiar Todo", this))
{
    setupUi();
    refreshLogs();

    connect(&Logger::instance(), &Logger::newLogEntry, this, &LogViewerWidget::appendLog);
}

void LogViewerWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Top Filter Bar
    auto* topLayout = new QHBoxLayout();
    auto* titleIcon = new QLabel(this);
    titleIcon->setPixmap(QIcon(":/icons/logs.svg").pixmap(20, 20));
    auto* titleLabel = new QLabel("REGISTRO Y LOGS DEL SISTEMA", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;");
    topLayout->addWidget(titleIcon);
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();

    m_edtSearch->setPlaceholderText("Buscar en los logs...");
    m_edtSearch->setStyleSheet("background: #1A1D24; color: white; border: 1px solid #374151; border-radius: 6px; padding: 6px 10px; font-size: 12px;");
    m_edtSearch->setFixedWidth(200);
    connect(m_edtSearch, &QLineEdit::textChanged, this, &LogViewerWidget::onFilterChanged);
    topLayout->addWidget(m_edtSearch);

    m_cmbLevel->addItem("Todos los niveles", -1);
    m_cmbLevel->addItem("INFO", static_cast<int>(LogLevel::INFO_LOG));
    m_cmbLevel->addItem("WARN", static_cast<int>(LogLevel::WARNING_LOG));
    m_cmbLevel->addItem("ERROR", static_cast<int>(LogLevel::ERROR_LOG));
    m_cmbLevel->addItem("DEBUG", static_cast<int>(LogLevel::DEBUG_LOG));
    m_cmbLevel->setStyleSheet("background: #1A1D24; color: white; border: 1px solid #374151; border-radius: 6px; padding: 6px 10px; font-size: 12px;");
    connect(m_cmbLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogViewerWidget::onFilterChanged);
    topLayout->addWidget(m_cmbLevel);

    m_btnCopy->setIcon(QIcon(":/icons/copy.svg"));
    m_btnCopy->setIconSize(QSize(14, 14));
    m_btnCopy->setCursor(Qt::PointingHandCursor);
    m_btnCopy->setStyleSheet("QPushButton { background: #374151; color: white; border: none; border-radius: 6px; padding: 6px 12px; font-size: 12px; } QPushButton:hover { background: #4B5563; }");
    connect(m_btnCopy, &QPushButton::clicked, this, &LogViewerWidget::onCopyClicked);
    topLayout->addWidget(m_btnCopy);

    m_btnClear->setIcon(QIcon(":/icons/trash.svg"));
    m_btnClear->setIconSize(QSize(14, 14));
    m_btnClear->setCursor(Qt::PointingHandCursor);
    m_btnClear->setStyleSheet("QPushButton { background: #374151; color: white; border: none; border-radius: 6px; padding: 6px 12px; font-size: 12px; } QPushButton:hover { background: #4B5563; }");
    connect(m_btnClear, &QPushButton::clicked, this, &LogViewerWidget::onClearClicked);
    topLayout->addWidget(m_btnClear);

    mainLayout->addLayout(topLayout);

    // Logs text area
    m_txtLogs->setReadOnly(true);
    m_txtLogs->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #0F172A;"
        "  color: #E2E8F0;"
        "  border: 1px solid #1E293B;"
        "  border-radius: 8px;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  padding: 10px;"
        "}"
    );
    mainLayout->addWidget(m_txtLogs, 1);
}

void LogViewerWidget::appendLog(const LogEntry& entry)
{
    int filterLevel = m_cmbLevel->currentData().toInt();
    if (filterLevel >= 0 && static_cast<int>(entry.level) != filterLevel)
    {
        return;
    }

    QString search = m_edtSearch->text().trimmed();
    if (!search.isEmpty() && !entry.formatted().contains(search, Qt::CaseInsensitive))
    {
        return;
    }

    m_txtLogs->appendPlainText(entry.formatted());
}

void LogViewerWidget::refreshLogs()
{
    m_txtLogs->clear();
    QList<LogEntry> logs = Logger::instance().getLogs();
    for (const auto& entry : logs)
    {
        appendLog(entry);
    }
}

void LogViewerWidget::onClearClicked()
{
    Logger::instance().clearLogs();
    m_txtLogs->clear();
}

void LogViewerWidget::onCopyClicked()
{
    QGuiApplication::clipboard()->setText(m_txtLogs->toPlainText());
}

void LogViewerWidget::onFilterChanged()
{
    refreshLogs();
}
