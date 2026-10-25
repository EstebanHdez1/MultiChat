#include "TikTokPlatform.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <zlib.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace
{
void appendVarint(QByteArray& target, quint64 value)
{
    do
    {
        quint8 byte = static_cast<quint8>(value & 0x7F);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        target.append(static_cast<char>(byte));
    } while (value != 0);
}

void appendLengthDelimited(QByteArray& target, int fieldNumber, const QByteArray& value)
{
    appendVarint(target, static_cast<quint64>(fieldNumber << 3 | 2));
    appendVarint(target, static_cast<quint64>(value.size()));
    target.append(value);
}

struct ProtoField
{
    int number = 0;
    int wire = 0;
    quint64 integer = 0;
    QByteArray bytes;
};

bool readProtoVarint(const QByteArray& bytes, int& position, quint64& value)
{
    value = 0;
    for (int shift = 0; position < bytes.size() && shift <= 63; shift += 7)
    {
        const quint8 byte = static_cast<quint8>(bytes[position++]);
        value |= static_cast<quint64>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) return true;
    }
    return false;
}

QList<ProtoField> readProtoFields(const QByteArray& bytes)
{
    QList<ProtoField> result;
    int position = 0;
    while (position < bytes.size() && result.size() < 4096)
    {
        quint64 tag = 0;
        if (!readProtoVarint(bytes, position, tag) || tag == 0) break;
        ProtoField field { static_cast<int>(tag >> 3), static_cast<int>(tag & 7), 0, {} };
        if (field.wire == 0)
        {
            if (!readProtoVarint(bytes, position, field.integer)) break;
        }
        else if (field.wire == 2)
        {
            quint64 length = 0;
            if (!readProtoVarint(bytes, position, length) || length > static_cast<quint64>(bytes.size() - position)) break;
            field.bytes = bytes.mid(position, static_cast<int>(length));
            position += static_cast<int>(length);
        }
        else if (field.wire == 1) position += 8;
        else if (field.wire == 5) position += 4;
        else break;
        if (position > bytes.size()) break;
        result.append(field);
    }
    return result;
}

QByteArray firstProtoBytes(const QList<ProtoField>& fields, int number)
{
    for (const ProtoField& field : fields)
        if (field.number == number && field.wire == 2) return field.bytes;
    return {};
}

quint64 firstProtoInteger(const QList<ProtoField>& fields, int number)
{
    for (const ProtoField& field : fields)
        if (field.number == number && field.wire == 0) return field.integer;
    return 0;
}

static QByteArray decompressGzipChunk(const QByteArray& data)
{
    if (data.size() < 4) return data;

    int offset = 0;
    // Check if starts with gzip magic (0x1F, 0x8B) or search first 32 bytes
    if (static_cast<quint8>(data[0]) != 0x1F || static_cast<quint8>(data[1]) != 0x8B)
    {
        const int scanLimit = qMin(32, data.size() - 2);
        for (int k = 0; k <= scanLimit; ++k)
        {
            if (static_cast<quint8>(data[k]) == 0x1F && static_cast<quint8>(data[k + 1]) == 0x8B)
            {
                offset = k;
                break;
            }
        }
    }

    z_stream strm{};
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data() + offset));
    strm.avail_in = static_cast<uInt>(data.size() - offset);

    if (inflateInit2(&strm, 32 + MAX_WBITS) != Z_OK)
    {
        return data;
    }

    QByteArray out;
    constexpr int MAX_DECOMPRESSED_BYTES = 8 * 1024 * 1024;
    char buffer[32768];
    int ret = Z_OK;
    while (ret == Z_OK)
    {
        strm.next_out = reinterpret_cast<Bytef*>(buffer);
        strm.avail_out = sizeof(buffer);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_OK || ret == Z_STREAM_END)
        {
            out.append(buffer, sizeof(buffer) - strm.avail_out);
            if (out.size() > MAX_DECOMPRESSED_BYTES)
            {
                inflateEnd(&strm);
                return {};
            }
        }
    }
    inflateEnd(&strm);

    return out.isEmpty() ? data : out;
}
} // namespace

TikTokPlatform::TikTokPlatform(QObject* parent)
    : BasePlatform("TikTok", parent)
    , m_cdpSocket(new WebSocketWrapper("TikTokCDP", this))
    , m_netManager(new QNetworkAccessManager(this))
    , m_domScrapeTimer(new QTimer(this))
{
    m_cdpSocket->setAutoReconnect(false);
    connect(m_cdpSocket, &WebSocketWrapper::connected, this, &TikTokPlatform::onCdpConnected);
    connect(m_cdpSocket, &WebSocketWrapper::disconnected, this, &TikTokPlatform::onCdpDisconnected);
    connect(m_cdpSocket, &WebSocketWrapper::textMessageReceived, this, &TikTokPlatform::onCdpTextMessage);

    connect(m_domScrapeTimer, &QTimer::timeout, this, &TikTokPlatform::onDomScrapeTimeout);
}

TikTokPlatform::~TikTokPlatform()
{
    doDisconnect();

    if (m_browserProcess)
    {
        m_browserProcess->kill();
        m_browserProcess->waitForFinished(1000);
        delete m_browserProcess;
        m_browserProcess = nullptr;
    }
}

void TikTokPlatform::startChatListener()
{
    doConnect();
}

void TikTokPlatform::stopChatListener()
{
    doDisconnect();
}

QString TikTokPlatform::findBrowserExecutable() const
{
    // 1. Google Chrome (Preferred)
    const QStringList chromePaths = {
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        QDir::homePath() + "\\AppData\\Local\\Google\\Chrome\\Application\\chrome.exe"
    };
    for (const QString& path : chromePaths)
    {
        if (QFile::exists(path)) return path;
    }

    // 2. Microsoft Edge (Chromium fallback)
    const QStringList edgePaths = {
        "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        QDir::homePath() + "\\AppData\\Local\\Microsoft\\Edge\\Application\\msedge.exe"
    };
    for (const QString& path : edgePaths)
    {
        if (QFile::exists(path)) return path;
    }

    // 3. System PATH
    QString inPath = QStandardPaths::findExecutable("chrome.exe");
    if (!inPath.isEmpty()) return inPath;
    inPath = QStandardPaths::findExecutable("msedge.exe");
    if (!inPath.isEmpty()) return inPath;

    return {};
}

void TikTokPlatform::checkCdpAvailable(std::function<void(bool isUp)> callback)
{
    QUrl url(QString("http://127.0.0.1:%1/json/version").arg(m_cdpPort));
    QNetworkRequest req(url);
    req.setTransferTimeout(1500);

    QNetworkReply* reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        bool ok = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        if (callback) callback(ok);
    });
}

void TikTokPlatform::ensureBrowserRunning(std::function<void(bool success)> callback)
{
    checkCdpAvailable([this, callback](bool alreadyUp) {
        if (alreadyUp)
        {
            Logger::instance().info("TikTok", "Instancia del navegador en segundo plano ya activa en puerto CDP 9222");
            if (callback) callback(true);
            return;
        }

        QString browserExe = findBrowserExecutable();
        if (browserExe.isEmpty())
        {
            Logger::instance().error("TikTok", "No se encontró Google Chrome ni Microsoft Edge instalado en el equipo.");
            setStatus(PlatformStatus::ERROR_STATE, "Navegador Chrome/Edge no encontrado");
            if (callback) callback(false);
            return;
        }

        QString browserName = QFileInfo(browserExe).baseName();
        Logger::instance().info("TikTok", QString("Iniciando %1 en segundo plano (modo invisible anti-bot)...").arg(browserName));
        setStatus(PlatformStatus::CONNECTING, QString("Iniciando %1...").arg(browserName));

        QString profileDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tiktok_browser_profile";
        QDir().mkpath(profileDir);

        QStringList args = {
            QString("--remote-debugging-port=%1").arg(m_cdpPort),
            "--remote-allow-origins=*",
            QString("--user-data-dir=%1").arg(profileDir),
            "--window-size=1280,720",
            "--mute-audio",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-background-timer-throttling",
            "--disable-backgrounding-occluded-windows",
            "--disable-renderer-backgrounding",
            "about:blank"
        };

        if (!m_browserProcess)
        {
            m_browserProcess = new QProcess(this);
        }
        else if (m_browserProcess->state() != QProcess::NotRunning)
        {
            m_browserProcess->kill();
            m_browserProcess->waitForFinished(500);
        }

        m_browserProcess->start(browserExe, args);

        // Poll until CDP port responds (up to 10 seconds)
        auto startTime = QDateTime::currentMSecsSinceEpoch();
        auto* pollTimer = new QTimer(this);
        pollTimer->setInterval(350);

        connect(pollTimer, &QTimer::timeout, this, [this, pollTimer, startTime, callback]() {
            checkCdpAvailable([this, pollTimer, startTime, callback](bool isUp) {
                if (isUp)
                {
                    pollTimer->stop();
                    pollTimer->deleteLater();
                    Logger::instance().info("TikTok", "Navegador iniciado y listo para conexiones CDP");
                    if (callback) callback(true);
                }
                else if (QDateTime::currentMSecsSinceEpoch() - startTime > 10000)
                {
                    pollTimer->stop();
                    pollTimer->deleteLater();
                    Logger::instance().error("TikTok", "Tiempo de espera agotado al iniciar el navegador.");
                    setStatus(PlatformStatus::ERROR_STATE, "Tiempo agotado al iniciar navegador");
                    if (callback) callback(false);
                }
            });
        });

        pollTimer->start();
    });
}

void TikTokPlatform::doConnect()
{
    m_username = m_channelName.trimmed();
    const QRegularExpression urlUser("tiktok\\.com/@([^/?#]+)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch urlMatch = urlUser.match(m_username);
    if (urlMatch.hasMatch())
    {
        m_username = urlMatch.captured(1);
    }
    if (m_username.startsWith('@'))
    {
        m_username = m_username.mid(1);
    }

    if (m_username.isEmpty())
    {
        setStatus(PlatformStatus::OFFLINE, "Usuario de TikTok no especificado");
        return;
    }

    setStatus(PlatformStatus::CONNECTING, "Preparando navegador...");
    m_isConnecting = true;

    ensureBrowserRunning([this](bool success) {
        if (!success || !m_isConnecting)
        {
            if (!success) setStatus(PlatformStatus::OFFLINE, "Error al iniciar navegador");
            return;
        }

        attachToLiveTab();
    });
}

void TikTokPlatform::attachToLiveTab()
{
    const QString liveUrl = QString("https://www.tiktok.com/@%1/live").arg(m_username);
    Logger::instance().info("TikTok", QString("Navegando hacia directo de TikTok: @%1").arg(m_username));
    setStatus(PlatformStatus::CONNECTING, QString("Cargando directo de @%1...").arg(m_username));

    // Request new tab via CDP HTTP PUT /json/new?<url>
    QUrl newTabUrl(QString("http://127.0.0.1:%1/json/new?%2").arg(m_cdpPort).arg(liveUrl));
    QNetworkRequest req(newTabUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray emptyBody;
    QNetworkReply* reply = m_netManager->put(req, emptyBody);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject())
        {
            Logger::instance().warn("TikTok", "No se pudo obtener información de la pestaña en el navegador.");
            setStatus(PlatformStatus::ERROR_STATE, "Fallo al crear pestaña en navegador");
            return;
        }

        QJsonObject obj = doc.object();
        m_targetId = obj["id"].toString();
        m_wsDebuggerUrl = obj["webSocketDebuggerUrl"].toString();

        if (m_wsDebuggerUrl.isEmpty())
        {
            Logger::instance().warn("TikTok", "Respuesta de CDP sin webSocketDebuggerUrl.");
            setStatus(PlatformStatus::ERROR_STATE, "URL de depuración de navegador no disponible");
            return;
        }

        Logger::instance().info("TikTok", QString("Conectando socket CDP a la pestaña: %1").arg(m_targetId));
        m_cdpSocket->connectUrl(QUrl(m_wsDebuggerUrl));
    });
}

void TikTokPlatform::doDisconnect()
{
    m_isConnecting = false;
    m_domScrapeTimer->stop();

    if (m_cdpSocket->isConnected())
    {
        m_cdpSocket->disconnectSocket();
    }

    if (!m_targetId.isEmpty())
    {
        // Close the live tab in browser via /json/close/<id>
        QUrl closeUrl(QString("http://127.0.0.1:%1/json/close/%2").arg(m_cdpPort).arg(m_targetId));
        QNetworkRequest req(closeUrl);
        QByteArray emptyBody;
        QNetworkReply* reply = m_netManager->put(req, emptyBody);
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
        m_targetId.clear();
    }

    m_seenMessageIds.clear();
    m_seenMessageOrder.clear();
    m_lastLikeByUser.clear();
    m_recentContentSigs.clear();
    m_lastWsMessageTime = 0;
    m_hasReceivedWebSocketFrame = false;

    setStatus(PlatformStatus::OFFLINE);
    setViewerCount(-1);
}

void TikTokPlatform::onCdpConnected()
{
    Logger::instance().info("TikTok", "Conectado exitosamente al protocolo de control del navegador (CDP)");
    setStatus(PlatformStatus::ONLINE);

    // Enable CDP domains
    sendCdpCommand("Page.enable");
    sendCdpCommand("Network.enable");
    sendCdpCommand("Runtime.enable");

    // Start DOM scraping timer for viewer counts, captcha detection, and DOM fallback
    m_domScrapeTimer->start(2500);
}

void TikTokPlatform::onCdpDisconnected()
{
    m_domScrapeTimer->stop();
    if (m_isConnecting)
    {
        Logger::instance().warn("TikTok", "Conexión con la pestaña del navegador cerrada.");
        setStatus(PlatformStatus::OFFLINE, "Pestaña cerrada");
    }
}

void TikTokPlatform::sendCdpCommand(const QString& method, const QJsonObject& params)
{
    if (!m_cdpSocket->isConnected()) return;

    QJsonObject cmd;
    cmd["id"] = m_cdpMsgId++;
    cmd["method"] = method;
    if (!params.isEmpty())
    {
        cmd["params"] = params;
    }

    QJsonDocument doc(cmd);
    m_cdpSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void TikTokPlatform::onCdpTextMessage(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    parseCdpMessage(doc.object());
}

void TikTokPlatform::parseCdpMessage(const QJsonObject& root)
{
    QString method = root["method"].toString();

    // Intercept WebSocket frames received by the browser
    if (method == "Network.webSocketFrameReceived")
    {
        m_hasReceivedWebSocketFrame = true;
        m_lastWsMessageTime = QDateTime::currentMSecsSinceEpoch();

        QJsonObject params = root["params"].toObject();
        QJsonObject response = params["response"].toObject();
        QString payloadData = response["payloadData"].toString();
        if (!payloadData.isEmpty())
        {
            QByteArray binaryData = QByteArray::fromBase64(payloadData.toLatin1());
            if (!binaryData.isEmpty())
            {
                parseWebcastResponse(binaryData);
            }
        }
    }
    else if (method == "Network.webSocketCreated")
    {
        QJsonObject params = root["params"].toObject();
        QString wsUrl = params["url"].toString();
        if (wsUrl.contains("webcast") || wsUrl.contains("tiktok"))
        {
            Logger::instance().info("TikTok", "Stream WebSocket de TikTok Webcast establecido en el navegador");
            setStatus(PlatformStatus::ONLINE);
        }
    }
    // Handle Runtime.evaluate responses (from DOM scraper)
    else if (root.contains("result"))
    {
        QJsonObject resObj = root["result"].toObject();
        if (resObj.contains("result"))
        {
            QJsonObject inner = resObj["result"].toObject();
            if (inner.value("type").toString() == "object" && inner.contains("value"))
            {
                QJsonValue val = inner.value("value");
                if (val.isObject())
                {
                    parseDomScrapeResult(val.toObject());
                }
            }
        }
    }
}

void TikTokPlatform::onDomScrapeTimeout()
{
    if (!m_cdpSocket->isConnected()) return;

    // JavaScript injected into the live page to extract live status, viewers, captcha alerts, and chat
    const QString script = R"JS(
        (() => {
            let viewerCount = -1;
            const allElements = document.querySelectorAll('*');
            for (const el of allElements) {
                const text = el.innerText || "";
                if (text.includes("Espectadores") || text.includes("Viewers")) {
                    const m = text.match(/(?:Espectadores|Viewers)\s*[·•]?\s*([0-9.,KMBkmb]+)/i);
                    if (m) {
                        let str = m[1].replace(/,/g, '').trim().toLowerCase();
                        if (str.endsWith('k')) viewerCount = Math.round(parseFloat(str) * 1000);
                        else if (str.endsWith('m')) viewerCount = Math.round(parseFloat(str) * 1000000);
                        else viewerCount = parseInt(str, 10) || -1;
                        break;
                    }
                }
            }

            const title = document.title || "";
            const isLive = title.includes("LIVE") || title.includes("emitiendo") || title.includes("live");
            const bodyText = document.body ? document.body.innerText : "";
            const isCaptcha = bodyText.includes("Verifica que eres humano") || 
                              bodyText.includes("Verifica si eres un robot") || 
                              bodyText.includes("SecSDK") ||
                              bodyText.includes("verify to continue");

            // Only extract DOM chat rows as safety net if WebSocket stream has not delivered frames yet
            const domMessages = [];
            return {
                viewerCount: viewerCount,
                title: title,
                isLive: isLive,
                isCaptcha: isCaptcha,
                domMessages: domMessages
            };
        })()
    )JS";

    QJsonObject params;
    params["expression"] = script;
    params["returnByValue"] = true;
    sendCdpCommand("Runtime.evaluate", params);
}

void TikTokPlatform::parseDomScrapeResult(const QJsonObject& result)
{
    int viewers = result["viewerCount"].toInt(-1);
    if (viewers >= 0)
    {
        setViewerCount(viewers);
    }

    bool isCaptcha = result["isCaptcha"].toBool(false);
    if (isCaptcha)
    {
        Logger::instance().warn("TikTok", "Atención: TikTok ha solicitado una verificación de robot interactiva. Si no recibes mensajes, puedes mostrar el navegador para resolverla.");
    }

    // Never process DOM chat messages once the WebSocket stream is active
    if (m_hasReceivedWebSocketFrame)
    {
        return;
    }

    // Process DOM fallback messages ONLY when WebSocket stream is inactive
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool wsActive = (m_lastWsMessageTime > 0 && (now - m_lastWsMessageTime) < 10000);
    if (!wsActive && result.contains("domMessages") && result["domMessages"].isArray())
    {
        QJsonArray msgs = result["domMessages"].toArray();
        for (const auto& v : msgs)
        {
            QJsonObject m = v.toObject();
            QString sender = m["sender"].toString().trimmed();
            QString text = m["text"].toString().trimmed();
            if (sender.isEmpty() || text.isEmpty()) continue;

            QString msgId = QString("dom_%1_%2").arg(sender, text);
            if (!rememberMessageId(msgId, sender, text)) continue;

            ChatMessage msg;
            msg.id = msgId;
            msg.platform = "TikTok";
            msg.displayName = sender;
            msg.username = sender;
            msg.message = text;
            msg.timestamp = QDateTime::currentDateTime();
            msg.userColor = "#FE2C55";
            msg.type = MessageType::NORMAL;

            if (text.contains("se ha unido", Qt::CaseInsensitive))
            {
                msg.type = MessageType::JOIN;
                msg.isTransient = true;
            }

            emitChatMessage(msg);
        }
    }
}

void TikTokPlatform::setBrowserVisible(bool visible)
{
    m_browserVisible = visible;

#ifdef Q_OS_WIN
    if (!m_browserProcess) return;
    DWORD pid = static_cast<DWORD>(m_browserProcess->processId());
    if (pid == 0) return;

    struct EnumData {
        DWORD targetPid;
        HWND foundHwnd;
    } data { pid, nullptr };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* d = reinterpret_cast<EnumData*>(lParam);
        DWORD wndPid = 0;
        GetWindowThreadProcessId(hwnd, &wndPid);
        if (wndPid == d->targetPid && IsWindow(hwnd))
        {
            // Prefer main window with title or style
            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            if (style & WS_VISIBLE)
            {
                d->foundHwnd = hwnd;
                return FALSE;
            }
            if (!d->foundHwnd) d->foundHwnd = hwnd;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    if (data.foundHwnd)
    {
        SetWindowPos(data.foundHwnd, HWND_TOP, 100, 100, 1280, 720, SWP_SHOWWINDOW);
        ShowWindow(data.foundHwnd, SW_RESTORE);
        SetForegroundWindow(data.foundHwnd);
        Logger::instance().info("TikTok", "Ventana del navegador visible en pantalla");
    }
#endif
}

bool TikTokPlatform::rememberMessageId(const QString& id, const QString& user, const QString& text)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!id.isEmpty() && m_seenMessageIds.contains(id)) return false;

    if (!user.isEmpty() && !text.isEmpty())
    {
        QString sig = QString("%1:%2").arg(user.toLower().trimmed(), text.trimmed());
        if (m_recentContentSigs.contains(sig))
        {
            if (now - m_recentContentSigs.value(sig) < 15000)
            {
                return false; // Deduplicate between WebSocket & DOM within 15s
            }
        }
        m_recentContentSigs[sig] = now;
        if (m_recentContentSigs.size() > 1000)
        {
            auto it = m_recentContentSigs.begin();
            while (it != m_recentContentSigs.end())
            {
                if (now - it.value() > 30000) it = m_recentContentSigs.erase(it);
                else ++it;
            }
        }
    }

    if (!id.isEmpty())
    {
        m_seenMessageIds.insert(id);
        m_seenMessageOrder.enqueue(id);
        while (m_seenMessageOrder.size() > 2500)
        {
            m_seenMessageIds.remove(m_seenMessageOrder.dequeue());
        }
    }
    return true;
}

void TikTokPlatform::parseWebcastResponse(const QByteArray& rawData)
{
    constexpr int MAX_WEBSOCKET_BATCH_BYTES = 4 * 1024 * 1024;
    if (rawData.size() > MAX_WEBSOCKET_BATCH_BYTES) return;

    QByteArray data = decompressGzipChunk(rawData);
    if (data.isEmpty()) return;

    // Check JSON response format
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject())
    {
        QJsonObject root = doc.object();
        QJsonObject dataObj = (root.contains("data") && root["data"].isObject()) 
            ? root["data"].toObject() 
            : root;

        QJsonArray messages;
        if (dataObj.contains("messages") && dataObj["messages"].isArray())
        {
            messages = dataObj["messages"].toArray();
        }
        else if (root.contains("messages") && root["messages"].isArray())
        {
            messages = root["messages"].toArray();
        }

        if (!messages.isEmpty())
        {
            for (const auto& msgVal : messages)
            {
                QJsonObject mObj = msgVal.toObject();
                QString method = mObj["method"].toString(mObj["type"].toString());

                if (method.contains("ChatMessage") || method.contains("Comment"))
                {
                    handleChatMessage(mObj);
                }
                else if (method.contains("GiftMessage") || method.contains("Gift"))
                {
                    handleGiftMessage(mObj);
                }
                else if (method.contains("MemberMessage") || method.contains("Subscribe"))
                {
                    handleMemberMessage(mObj);
                }
                else if (method.contains("SocialMessage") || method.contains("Share"))
                {
                    handleSocialMessage(mObj);
                }
                else if (method.contains("LikeMessage") || method.contains("Like"))
                {
                    handleLikeMessage(mObj);
                }
                else if (method.contains("RoomUserSeq") || method.contains("UserSeq"))
                {
                    int viewers = mObj["total"].toInt(mObj["user_count"].toInt(-1));
                    if (viewers >= 0)
                    {
                        setViewerCount(viewers);
                    }
                }
            }
            return;
        }
    }

    // Binary Protobuf Scanner
    parseBinaryProtobuf(data);
}

void TikTokPlatform::parseBinaryProtobuf(const QByteArray& data)
{
    if (data.isEmpty() || data.size() > 4 * 1024 * 1024) return;

    QByteArray protoData = data;
    QList<ProtoField> response = readProtoFields(protoData);

    // Check PushFrame envelope
    QByteArray framePayload = firstProtoBytes(response, 8);
    const QString encoding = QString::fromUtf8(firstProtoBytes(response, 6));
    const QString payloadType = QString::fromUtf8(firstProtoBytes(response, 7));
    const quint64 logId = firstProtoInteger(response, 2);

    if (!framePayload.isEmpty() && (encoding == "pb" || encoding == "gzip" || payloadType == "msg" || payloadType == "im_enter_room" || logId > 0 || !firstProtoBytes(response, 7).isEmpty()))
    {
        protoData = decompressGzipChunk(framePayload);
        response = readProtoFields(protoData);
    }
    else
    {
        QByteArray decomp = decompressGzipChunk(protoData);
        if (decomp != protoData)
        {
            protoData = decomp;
            response = readProtoFields(protoData);
        }
    }

    // In WebcastResponse: field 1 contains messages list
    for (const ProtoField& envelope : response)
    {
        if (envelope.number != 1 || envelope.wire != 2) continue;
        const QList<ProtoField> base = readProtoFields(envelope.bytes);
        const QString method = QString::fromUtf8(firstProtoBytes(base, 1));

        // Chat Message
        if (method.contains("ChatMessage", Qt::CaseInsensitive))
        {
            const QByteArray payload = firstProtoBytes(base, 2);
            const QList<ProtoField> chat = readProtoFields(payload);

            QString content = QString::fromUtf8(firstProtoBytes(chat, 3)).trimmed();
            if (content.isEmpty() && method.contains("Emote", Qt::CaseInsensitive))
            {
                const QList<ProtoField> emoteField = readProtoFields(firstProtoBytes(chat, 3));
                content = QString::fromUtf8(firstProtoBytes(emoteField, 3)).trimmed();
                if (content.isEmpty()) content = QString::fromUtf8(firstProtoBytes(chat, 4)).trimmed();
                if (content.isEmpty()) content = "[Emote]";
            }
            if (content.isEmpty())
            {
                const QList<ProtoField> common = readProtoFields(firstProtoBytes(chat, 1));
                content = QString::fromUtf8(firstProtoBytes(common, 6)).trimmed();
            }
            if (content.isEmpty()) continue;

            const QList<ProtoField> user = readProtoFields(firstProtoBytes(chat, 2));
            QString displayName = QString::fromUtf8(firstProtoBytes(user, 3)).trimmed();
            QString username = QString::fromUtf8(firstProtoBytes(user, 5)).trimmed();
            static const QRegularExpression s_badgeRegex("^N[.°\\s]*\\d+$", QRegularExpression::CaseInsensitiveOption);
            if (displayName.contains(s_badgeRegex)) displayName.clear();
            if (username.contains(s_badgeRegex)) username.clear();
            if (username.isEmpty()) username = QString::fromUtf8(firstProtoBytes(user, 38)).trimmed();
            if (username.contains(s_badgeRegex)) username.clear();
            if (username.isEmpty()) username = displayName;
            if (displayName.isEmpty()) displayName = username;
            if (displayName.isEmpty()) displayName = "TikToker";
            if (username.isEmpty()) username = displayName;

            const QList<ProtoField> common = readProtoFields(firstProtoBytes(chat, 1));
            QString messageId = QString::number(firstProtoInteger(common, 2));
            if (messageId == "0") messageId = QString::number(firstProtoInteger(base, 3));
            if (messageId == "0") messageId = QString("tt_%1_%2_%3").arg(username, content, QString::number(QDateTime::currentMSecsSinceEpoch()));
            if (!rememberMessageId(messageId, username, content)) continue;
            rememberMessageId(QString(), displayName, content);
            m_lastWsMessageTime = QDateTime::currentMSecsSinceEpoch();

            ChatMessage message;
            message.id = messageId;
            message.platform = "TikTok";
            message.username = username;
            message.displayName = displayName;
            message.message = content;
            message.timestamp = QDateTime::currentDateTime();
            message.userColor = "#FE2C55";
            message.type = MessageType::NORMAL;
            emitChatMessage(message);
        }
        // Gift Message
        else if (method.contains("GiftMessage", Qt::CaseInsensitive))
        {
            const QByteArray payload = firstProtoBytes(base, 2);
            const QList<ProtoField> gift = readProtoFields(payload);
            const QList<ProtoField> user = readProtoFields(firstProtoBytes(gift, 7));
            QString displayName = QString::fromUtf8(firstProtoBytes(user, 3)).trimmed();
            QString username = QString::fromUtf8(firstProtoBytes(user, 5)).trimmed();
            if (username.isEmpty()) username = QString::fromUtf8(firstProtoBytes(user, 38)).trimmed();
            if (username.isEmpty()) username = displayName;
            if (displayName.isEmpty()) displayName = username;

            const QList<ProtoField> giftDetails = readProtoFields(firstProtoBytes(gift, 15));
            QString giftName = QString::fromUtf8(firstProtoBytes(giftDetails, 2)).trimmed();
            if (giftName.isEmpty()) giftName = "Regalo";
            int diamonds = static_cast<int>(firstProtoInteger(giftDetails, 12));
            if (diamonds <= 0) diamonds = 1;
            int repeatCount = static_cast<int>(firstProtoInteger(gift, 5));
            if (repeatCount <= 0) repeatCount = 1;
            int total = diamonds * repeatCount;

            const QList<ProtoField> common = readProtoFields(firstProtoBytes(gift, 1));
            QString messageId = QString::number(firstProtoInteger(common, 2));
            if (messageId == "0") messageId = QString::number(firstProtoInteger(base, 3));
            if (messageId == "0") messageId = QString("ttgift_%1_%2_%3").arg(username, giftName, QString::number(QDateTime::currentMSecsSinceEpoch()));
            if (!rememberMessageId(messageId)) continue;

            Donation don;
            don.id = messageId;
            don.platform = "TikTok";
            don.username = username;
            don.displayName = displayName;
            don.amount = total;
            don.currency = "Coins";
            don.type = QString("TikTok Gift (%1)").arg(giftName);
            don.message = QString("¡Ha enviado %1x %2! (%3 Monedas)").arg(repeatCount).arg(giftName).arg(total);
            don.timestamp = QDateTime::currentDateTime();

            ChatMessage message;
            message.id = messageId;
            message.platform = "TikTok";
            message.username = username;
            message.displayName = displayName;
            message.message = don.message;
            message.timestamp = don.timestamp;
            message.userColor = "#FE2C55";
            message.type = MessageType::GIFT;
            message.isDonation = true;
            message.donationAmount = total;
            message.currency = "Coins";
            message.giftName = giftName;
            message.giftCount = repeatCount;

            m_lastWsMessageTime = QDateTime::currentMSecsSinceEpoch();
            emitDonation(don);
            emitChatMessage(message);
        }
        // Member / Join Message
        else if (method.contains("MemberMessage", Qt::CaseInsensitive))
        {
            const AppStyleConfig cfg = ConfigManager::instance().getStyleConfig();
            if (!cfg.showJoinEvents) continue;

            const QByteArray payload = firstProtoBytes(base, 2);
            const QList<ProtoField> member = readProtoFields(payload);
            const QList<ProtoField> user = readProtoFields(firstProtoBytes(member, 2));
            QString displayName = QString::fromUtf8(firstProtoBytes(user, 3)).trimmed();
            QString username = QString::fromUtf8(firstProtoBytes(user, 5)).trimmed();
            if (username.isEmpty()) username = QString::fromUtf8(firstProtoBytes(user, 38)).trimmed();
            if (username.isEmpty()) username = displayName;
            if (displayName.isEmpty()) displayName = username;

            const QList<ProtoField> common = readProtoFields(firstProtoBytes(member, 1));
            QString messageId = QString::number(firstProtoInteger(common, 2));
            if (messageId == "0") messageId = QString::number(firstProtoInteger(base, 3));
            if (messageId == "0") messageId = QString("ttmem_%1_%2").arg(username, QString::number(QDateTime::currentMSecsSinceEpoch()));
            if (!rememberMessageId(messageId)) continue;

            ChatMessage message;
            message.id = messageId;
            message.platform = "TikTok";
            message.username = username;
            message.displayName = displayName;
            message.message = "¡Se ha unido al directo!";
            message.timestamp = QDateTime::currentDateTime();
            message.userColor = "#FE2C55";
            message.type = MessageType::JOIN;
            message.isTransient = true;
            emitChatMessage(message);
        }
        // Social Message (Share / Follow)
        else if (method.contains("SocialMessage", Qt::CaseInsensitive))
        {
            const AppStyleConfig cfg = ConfigManager::instance().getStyleConfig();
            const QByteArray payload = firstProtoBytes(base, 2);
            const QList<ProtoField> social = readProtoFields(payload);
            quint64 action = firstProtoInteger(social, 3);
            if (action == 1 && !cfg.showFollowEvents) continue;
            if (action != 1 && !cfg.showShareEvents) continue;

            const QList<ProtoField> user = readProtoFields(firstProtoBytes(social, 2));
            QString displayName = QString::fromUtf8(firstProtoBytes(user, 3)).trimmed();
            QString username = QString::fromUtf8(firstProtoBytes(user, 5)).trimmed();
            if (username.isEmpty()) username = QString::fromUtf8(firstProtoBytes(user, 38)).trimmed();
            if (username.isEmpty()) username = displayName;
            if (displayName.isEmpty()) displayName = username;

            const QList<ProtoField> common = readProtoFields(firstProtoBytes(social, 1));
            QString messageId = QString::number(firstProtoInteger(common, 2));
            if (messageId == "0") messageId = QString::number(firstProtoInteger(base, 3));
            if (messageId == "0") messageId = QString("ttsoc_%1_%2").arg(username, QString::number(QDateTime::currentMSecsSinceEpoch()));
            if (!rememberMessageId(messageId)) continue;

            ChatMessage message;
            message.id = messageId;
            message.platform = "TikTok";
            message.username = username;
            message.displayName = displayName;
            message.timestamp = QDateTime::currentDateTime();
            message.userColor = "#FE2C55";
            message.isTransient = true;

            if (action == 1)
            {
                message.type = MessageType::FOLLOW;
                message.message = "¡Ha comenzado a seguir al streamer!";
            }
            else
            {
                message.type = MessageType::SHARE;
                message.message = "¡Ha compartido el directo!";
            }
            emitChatMessage(message);
        }
        // Like Message
        else if (method.contains("LikeMessage", Qt::CaseInsensitive))
        {
            const AppStyleConfig cfg = ConfigManager::instance().getStyleConfig();
            if (!cfg.showLikeEvents) continue;

            const QByteArray payload = firstProtoBytes(base, 2);
            const QList<ProtoField> like = readProtoFields(payload);
            const QList<ProtoField> user = readProtoFields(firstProtoBytes(like, 5));
            QString displayName = QString::fromUtf8(firstProtoBytes(user, 3)).trimmed();
            QString username = QString::fromUtf8(firstProtoBytes(user, 5)).trimmed();
            if (username.isEmpty()) username = QString::fromUtf8(firstProtoBytes(user, 38)).trimmed();
            if (username.isEmpty()) username = displayName;
            if (displayName.isEmpty()) displayName = username;

            // Anti-spam cooldown per user
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (m_lastLikeByUser.contains(username))
            {
                qint64 lastTime = m_lastLikeByUser.value(username);
                int cooldownMs = qMax(2, cfg.likeCooldownSec) * 1000;
                if (now - lastTime < cooldownMs)
                {
                    continue;
                }
            }
            m_lastLikeByUser[username] = now;
            if (m_lastLikeByUser.size() > 500) m_lastLikeByUser.clear();

            int count = static_cast<int>(firstProtoInteger(like, 2));
            if (count <= 0) count = 1;

            const QList<ProtoField> common = readProtoFields(firstProtoBytes(like, 1));
            QString messageId = QString::number(firstProtoInteger(common, 2));
            if (messageId == "0") messageId = QString::number(firstProtoInteger(base, 3));
            if (messageId == "0") messageId = QString("ttlike_%1_%2").arg(username, QString::number(QDateTime::currentMSecsSinceEpoch()));
            if (!rememberMessageId(messageId)) continue;

            ChatMessage message;
            message.id = messageId;
            message.platform = "TikTok";
            message.username = username;
            message.displayName = displayName;
            if (cfg.showLikeCount && count > 0)
            {
                message.message = QString("¡ha enviado %1 likes! ❤️").arg(count);
            }
            else
            {
                message.message = "¡dio like al directo! ❤️";
            }
            message.timestamp = QDateTime::currentDateTime();
            message.userColor = "#FE2C55";
            message.type = MessageType::LIKE;
            message.isTransient = true;
            m_lastWsMessageTime = QDateTime::currentMSecsSinceEpoch();
            emitChatMessage(message);
        }
        // Room User Sequence / Viewer count
        else if (method.contains("RoomUserSeqMessage", Qt::CaseInsensitive))
        {
            const QByteArray payload = firstProtoBytes(base, 2);
            const QList<ProtoField> fields = readProtoFields(payload);
            quint64 viewers = firstProtoInteger(fields, 4);
            if (viewers == 0) viewers = firstProtoInteger(fields, 3);
            if (viewers > 0)
            {
                setViewerCount(static_cast<int>(viewers));
            }
        }
    }
}

void TikTokPlatform::handleChatMessage(const QJsonObject& msgObj)
{
    QJsonObject user = msgObj["user"].toObject();
    ChatMessage msg;
    msg.platform = "TikTok";
    msg.displayName = user["nickname"].toString(user["display_id"].toString(user["unique_id"].toString("TikToker")));
    msg.username = user["display_id"].toString(user["unique_id"].toString(msg.displayName));
    msg.message = msgObj["content"].toString(msgObj["comment"].toString(msgObj["text"].toString()));

    QString msgId = msgObj["msg_id"].toString(msgObj["id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch())));
    if (!rememberMessageId(msgId, msg.username, msg.message)) return;

    msg.id = msgId;
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = "#FE2C55";
    msg.type = MessageType::NORMAL;

    if (!msg.message.isEmpty())
    {
        emitChatMessage(msg);
    }
}

void TikTokPlatform::handleGiftMessage(const QJsonObject& msgObj)
{
    QJsonObject user = msgObj["user"].toObject();
    QJsonObject gift = msgObj["gift"].toObject();

    QString msgId = msgObj["msg_id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    QString giftName = gift["name"].toString(gift["describe"].toString("Regalo"));
    int diamondCount = gift["diamond_count"].toInt(1);
    int repeatCount = msgObj["repeat_count"].toInt(msgObj["combo_count"].toInt(1));
    int totalDiamonds = diamondCount * repeatCount;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "TikTok";
    msg.displayName = user["nickname"].toString(user["display_id"].toString("TikToker"));
    msg.username = user["display_id"].toString(msg.displayName);
    msg.message = QString("¡Ha enviado %1x %2! (%3 Monedas)").arg(repeatCount).arg(giftName).arg(totalDiamonds);
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = "#FE2C55";
    msg.type = MessageType::GIFT;
    msg.isDonation = true;
    msg.donationAmount = totalDiamonds;
    msg.currency = "Coins";
    msg.giftName = giftName;
    msg.giftCount = repeatCount;

    Donation don;
    don.id = msg.id;
    don.platform = "TikTok";
    don.username = msg.username;
    don.displayName = msg.displayName;
    don.amount = totalDiamonds;
    don.currency = "Coins";
    don.type = QString("TikTok Gift (%1)").arg(giftName);
    don.message = msg.message;
    don.timestamp = msg.timestamp;

    emitDonation(don);
    emitChatMessage(msg);
}

void TikTokPlatform::handleMemberMessage(const QJsonObject& msgObj)
{
    const auto& cfg = ConfigManager::instance().getStyleConfig();
    if (!cfg.showJoinEvents) return;

    QJsonObject user = msgObj["user"].toObject();

    QString msgId = msgObj["msg_id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "TikTok";
    msg.displayName = user["nickname"].toString(user["display_id"].toString("TikToker"));
    msg.username = user["display_id"].toString(msg.displayName);
    msg.message = "¡Se ha unido al directo!";
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = "#FE2C55";
    msg.type = MessageType::JOIN;
    msg.isTransient = true;

    emitChatMessage(msg);
}

void TikTokPlatform::handleSocialMessage(const QJsonObject& msgObj)
{
    const auto& cfg = ConfigManager::instance().getStyleConfig();
    QJsonObject user = msgObj["user"].toObject();

    QString msgId = msgObj["msg_id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    int action = msgObj["action"].toInt(0);
    if (action == 1 && !cfg.showFollowEvents) return;
    if (action != 1 && !cfg.showShareEvents) return;

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "TikTok";
    msg.displayName = user["nickname"].toString(user["display_id"].toString("TikToker"));
    msg.username = user["display_id"].toString(msg.displayName);
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = "#FE2C55";
    msg.isTransient = true;

    if (action == 1)
    {
        msg.type = MessageType::FOLLOW;
        msg.message = "¡Ha comenzado a seguir al streamer!";
    }
    else
    {
        msg.type = MessageType::SHARE;
        msg.message = "¡Ha compartido el directo!";
    }

    emitChatMessage(msg);
}

void TikTokPlatform::handleLikeMessage(const QJsonObject& msgObj)
{
    const auto& cfg = ConfigManager::instance().getStyleConfig();
    if (!cfg.showLikeEvents) return;

    QJsonObject user = msgObj["user"].toObject();
    QString username = user["display_id"].toString(user["unique_id"].toString());
    if (username.isEmpty()) username = user["nickname"].toString();

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastLikeByUser.contains(username))
    {
        qint64 lastTime = m_lastLikeByUser.value(username);
        int cooldownMs = qMax(2, cfg.likeCooldownSec) * 1000;
        if (now - lastTime < cooldownMs) return;
    }
    m_lastLikeByUser[username] = now;
    if (m_lastLikeByUser.size() > 500) m_lastLikeByUser.clear();

    QString msgId = msgObj["msg_id"].toString(QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!rememberMessageId(msgId)) return;

    int count = msgObj["count"].toInt(1);

    ChatMessage msg;
    msg.id = msgId;
    msg.platform = "TikTok";
    msg.displayName = user["nickname"].toString(username);
    msg.username = username;
    if (cfg.showLikeCount && count > 0)
    {
        msg.message = QString("¡ha enviado %1 likes!").arg(count);
    }
    else
    {
        msg.message = "¡dio like al directo!";
    }
    msg.timestamp = QDateTime::currentDateTime();
    msg.userColor = "#FE2C55";
    msg.type = MessageType::LIKE;
    msg.isTransient = true;

    emitChatMessage(msg);
}
