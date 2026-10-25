#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ChatMessage.h"
#include "PlatformStatus.h"
#include "AppStyleConfig.h"
#include "ChatManager.h"

class LocalWebServer : public QObject
{
    Q_OBJECT

public:
    static LocalWebServer& instance();

    bool startServer(quint16 httpPort = 8080, quint16 wsPort = 8081);
    void stopServer();
    bool isRunning() const { return m_isRunning; }

    quint16 httpPort() const { return m_httpPort; }
    quint16 wsPort() const { return m_wsPort; }

    QString getLocalIpAddress() const;
    QString getLocalUrl() const;
    QString getNetworkUrl() const;
    QString getOverlayUrl() const;
    int clientCount() const;

signals:
    void serverStarted(const QString& localUrl, const QString& networkUrl);
    void serverStopped();
    void clientCountChanged(int count);
    void errorOccurred(const QString& errorMessage);

public slots:
    void onChatMessageReceived(const ChatMessage& msg, bool allowed);
    void onChatCleared();
    void onFilterChanged(const ChatFilter& filter);
    void onStyleChanged(const AppStyleConfig& style);
    void onViewerCountUpdated(const QString& platform, int count, int total);
    void onPlatformStatusChanged(const QString& platform, PlatformStatus status, const QString& detail);

private slots:
    void onNewHttpConnection();
    void onNewWebSocketConnection();
    void onWebSocketDisconnected();

private:
    explicit LocalWebServer(QObject* parent = nullptr);
    ~LocalWebServer() override;

    void handleHttpRequest(QTcpSocket* socket);
    void broadcastJson(const QJsonObject& json);
    QByteArray getIndexHtml() const;
    QJsonObject messageToJson(const ChatMessage& msg) const;
    QJsonObject styleToJson(const AppStyleConfig& style) const;
    QJsonObject filterToJson(const ChatFilter& filter) const;

    QTcpServer* m_httpServer = nullptr;
    QWebSocketServer* m_wsServer = nullptr;
    QList<QWebSocket*> m_wsClients;

    quint16 m_httpPort = 8080;
    quint16 m_wsPort = 8081;
    bool m_isRunning = false;
};
