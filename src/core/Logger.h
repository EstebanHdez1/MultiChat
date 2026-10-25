#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QList>

enum class LogLevel
{
    INFO_LOG,
    WARNING_LOG,
    ERROR_LOG,
    DEBUG_LOG
};

struct LogEntry
{
    QDateTime timestamp;
    LogLevel level;
    QString source;
    QString message;
    
    QString formatted() const;
};

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger& instance();

    void log(LogLevel level, const QString& source, const QString& message);
    void info(const QString& source, const QString& message);
    void warn(const QString& source, const QString& message);
    void error(const QString& source, const QString& message);
    void debug(const QString& source, const QString& message);

    QList<LogEntry> getLogs() const;
    void clearLogs();

signals:
    void newLogEntry(const LogEntry& entry);

private:
    Logger(QObject* parent = nullptr);
    ~Logger() override = default;
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable QMutex m_mutex;
    QList<LogEntry> m_logs;
    const int m_maxLogs = 2000;
};
