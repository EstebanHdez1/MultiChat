#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <functional>
#include <QMap>

class HttpHelper : public QObject
{
    Q_OBJECT

public:
    static HttpHelper& instance();

    using ResponseCallback = std::function<void(bool success, int statusCode, const QByteArray& responseData, const QString& errorString)>;

    void get(const QString& url, 
             const QMap<QString, QString>& headers, 
             ResponseCallback callback);

    void post(const QString& url, 
              const QMap<QString, QString>& headers, 
              const QByteArray& postData, 
              ResponseCallback callback);

private:
    HttpHelper(QObject* parent = nullptr);
    ~HttpHelper() override = default;

    QNetworkAccessManager m_nam;
};
