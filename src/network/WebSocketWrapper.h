#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QUrl>
#include "PlatformStatus.h"

class WebSocketWrapper : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketWrapper(const QString& name, QObject* parent = nullptr);
    ~WebSocketWrapper() override;

    void connectUrl(const QUrl& url, const QMap<QString, QString>& headers = {});
    void disconnectSocket();
    void sendTextMessage(const QString& message);
    void sendBinaryMessage(const QByteArray& data);

    bool isConnected() const;
    void setAutoReconnect(bool enable, int initialIntervalMs = 2000, int maxIntervalMs = 30000);

signals:
    void connected();
    void disconnected();
    void textMessageReceived(const QString& message);
    void binaryMessageReceived(const QByteArray& message);
    void errorOccurred(const QString& errorString);
    void statusChanged(PlatformStatus status);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onBinaryMessageReceived(const QByteArray& message);
    void onError(QAbstractSocket::SocketError error);
    void onReconnectTimeout();

private:
    QString m_name;
    QWebSocket* m_socket;
    QUrl m_targetUrl;
    QMap<QString, QString> m_headers;
    
    // Auto reconnect
    bool m_autoReconnect = true;
    bool m_manualClose = false;
    QTimer* m_reconnectTimer;
    int m_reconnectIntervalMs = 2000;
    int m_initialIntervalMs = 2000;
    int m_maxIntervalMs = 30000;
};
