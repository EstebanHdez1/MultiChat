#include "Logger.h"
#include <QDebug>
#include <iostream>

QString LogEntry::formatted() const
{
    QString timeStr = timestamp.toString("HH:mm:ss");
    QString levelStr;
    switch (level)
    {
    case LogLevel::INFO_LOG:    levelStr = "INFO"; break;
    case LogLevel::WARNING_LOG: levelStr = "WARN"; break;
    case LogLevel::ERROR_LOG:   levelStr = "ERROR"; break;
    case LogLevel::DEBUG_LOG:   levelStr = "DEBUG"; break;
    }
    
    return QString("[%1] [%2] [%3] %4").arg(timeStr, levelStr, source, message);
}

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

void Logger::log(LogLevel level, const QString& source, const QString& message)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.source = source;
    entry.message = message;

    {
        QMutexLocker locker(&m_mutex);
        m_logs.append(entry);
        if (m_logs.size() > m_maxLogs)
        {
            m_logs.removeFirst();
        }
    }

    std::cout << entry.formatted().toStdString() << std::endl;
    emit newLogEntry(entry);
}

void Logger::info(const QString& source, const QString& message)
{
    log(LogLevel::INFO_LOG, source, message);
}

void Logger::warn(const QString& source, const QString& message)
{
    log(LogLevel::WARNING_LOG, source, message);
}

void Logger::error(const QString& source, const QString& message)
{
    log(LogLevel::ERROR_LOG, source, message);
}

void Logger::debug(const QString& source, const QString& message)
{
    log(LogLevel::DEBUG_LOG, source, message);
}

QList<LogEntry> Logger::getLogs() const
{
    QMutexLocker locker(&m_mutex);
    return m_logs;
}

void Logger::clearLogs()
{
    QMutexLocker locker(&m_mutex);
    m_logs.clear();
}
