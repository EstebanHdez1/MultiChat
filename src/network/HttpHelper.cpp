#include "HttpHelper.h"
#include <QNetworkReply>

HttpHelper& HttpHelper::instance()
{
    static HttpHelper s_instance;
    return s_instance;
}

HttpHelper::HttpHelper(QObject* parent)
    : QObject(parent)
{
}

void HttpHelper::get(const QString& url, 
                    const QMap<QString, QString>& headers, 
                    ResponseCallback callback)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    
    // Default headers
    request.setHeader(QNetworkRequest::UserAgentHeader, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);

    for (auto it = headers.begin(); it != headers.end(); ++it)
    {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    QNetworkReply* reply = m_nam.get(request);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError || (statusCode >= 200 && statusCode < 300))
        {
            QByteArray data = reply->readAll();
            callback(true, statusCode, data, QString());
        }
        else
        {
            QString err = reply->errorString();
            QByteArray data = reply->readAll();
            callback(false, statusCode, data, err);
        }
    });
}

void HttpHelper::post(const QString& url, 
                     const QMap<QString, QString>& headers, 
                     const QByteArray& postData, 
                     ResponseCallback callback)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    
    request.setHeader(QNetworkRequest::UserAgentHeader, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);

    for (auto it = headers.begin(); it != headers.end(); ++it)
    {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    QNetworkReply* reply = m_nam.post(request, postData);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError || (statusCode >= 200 && statusCode < 300))
        {
            QByteArray data = reply->readAll();
            callback(true, statusCode, data, QString());
        }
        else
        {
            QString err = reply->errorString();
            QByteArray data = reply->readAll();
            callback(false, statusCode, data, err);
        }
    });
}
