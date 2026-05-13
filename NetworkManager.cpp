#include "NetworkManager.h"

#include <QNetworkInterface>
#include <QHostAddress>
#include <QDataStream>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <QDebug>
#include <QMutexLocker>
#include <QUrl>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <cstring>
#include <chrono>

// ============================================================================
//  Static Helpers
// ============================================================================

static bool sendAll(QTcpSocket* sock, const char* buf, qint64 len)
{
    qint64 sent = 0;
    while (sent < len) {
        if (QThread::currentThread()->isInterruptionRequested())
            return false;
        qint64 n = sock->write(buf + sent, len - sent);
        if (n < 0) return false;
        QElapsedTimer timer;
        timer.start();
        while (sock->bytesToWrite() > 0) {
            if (QThread::currentThread()->isInterruptionRequested())
                return false;
            if (sock->waitForBytesWritten(250))
                break;
            if (timer.elapsed() >= 30000)
                return false;
        }
        sent += n;
    }
    return true;
}

static bool recvAll(QTcpSocket* sock, char* buf, qint64 len)
{
    qint64 recvd = 0;
    while (recvd < len) {
        while (sock->bytesAvailable() < 1) {
            if (QThread::currentThread()->isInterruptionRequested())
                return false;
            QElapsedTimer timer;
            timer.start();
            while (sock->bytesAvailable() < 1) {
                if (QThread::currentThread()->isInterruptionRequested())
                    return false;
                if (sock->waitForReadyRead(250))
                    break;
                if (timer.elapsed() >= 30000)
                    return false;
            }
        }
        qint64 n = sock->read(buf + recvd, len - recvd);
        if (n <= 0) return false;
        recvd += n;
    }
    return true;
}

static double calcMbps(qint64 bytes, double seconds)
{
    if (seconds <= 0.0) return 0.0;
    return (bytes / 1024.0 / 1024.0) / seconds;
}

static constexpr qint64 chunkSizeForMode(bool isLan)
{
    return isLan ? LAN_CHUNK_SIZE : ECS_CHUNK_SIZE;
}

static bool looksLikeLegacyLanPeer(const QString& peer)
{
    const QString clean = peer.trimmed();
    if (clean.contains(QStringLiteral(" (")))
        return true;
    static const QRegularExpression ipv4Endpoint(QStringLiteral("^\\d{1,3}(?:\\.\\d{1,3}){3}(?::\\d+)?$"));
    return ipv4Endpoint.match(clean).hasMatch();
}

static bool isLanChatRecord(const QVariantMap& msg)
{
    if (msg.value(QStringLiteral("isLan")).toBool())
        return true;
    return looksLikeLegacyLanPeer(msg.value(QStringLiteral("from")).toString())
        || looksLikeLegacyLanPeer(msg.value(QStringLiteral("to")).toString());
}

static void tuneDataSocket(QTcpSocket* sock)
{
    static constexpr int BufferSize = 4 * 1024 * 1024;
    if (!sock) return;
    sock->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, BufferSize);
    sock->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, BufferSize);
}

static void clearDirectoryContents(const QString& path)
{
    QDir dir(path);
    if (!dir.exists())
        return;

    const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries) {
        if (entry.isDir())
            QDir(entry.absoluteFilePath()).removeRecursively();
        else
            QFile::remove(entry.absoluteFilePath());
    }
}

static QString safeFileNameToken(const QString& fileName)
{
    QString safe = QFileInfo(fileName).fileName().trimmed();
    safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]+")), QStringLiteral("_"));
    safe.replace(QRegularExpression(QStringLiteral("^\\.+$")), QStringLiteral("_"));
    if (safe.isEmpty())
        safe = QStringLiteral("unnamed_file");
    return safe.left(180);
}

static QString commandToken(const QStringList& parts, int index)
{
    return index >= 0 && index < parts.size() ? parts[index] : QString();
}

// ============================================================================
//  SendWorkerThread  —  runs in its own QThread, owns its data-channel socket
// ============================================================================

class SendWorkerThread : public QThread {
    Q_OBJECT
public:
    SendWorkerThread(QSharedPointer<SendTaskInfo> task,
                     const QString& dataDir, QObject* parent = nullptr)
        : QThread(parent), m_task(task), m_dataDir(dataDir) {}

signals:
    void taskProgress(int taskId, qint64 bytesSent, double speedMBps);
    void taskFinished(int taskId);
    void taskError(int taskId, const QString& msg);
    void sendCtrlMessage(const QString& msg);

protected:
    void run() override
    {
        // --- Connect data channel ---
        QTcpSocket dataSock;
        tuneDataSocket(&dataSock);
        QString host;
        int port;

        if (m_task->isLan) {
            host = m_task->lanPeerIp;
            port = m_task->lanDataPort;
        } else {
            host = QStringLiteral(ECS_SERVER_IP);
            port = ECS_DATA_PORT;
        }

        dataSock.connectToHost(host, port);
        if (!dataSock.waitForConnected(10000)) {
            emit taskError(m_task->taskId,
                           QStringLiteral("\u6570\u636e\u8fde\u63a5\u5931\u8d25: ") + dataSock.errorString());
            return;
        }

        // --- Handshake ---
        QString hello;
        if (m_task->isLan)
            hello = QStringLiteral("LAN_DATA_HANDSHAKE %1\n").arg(m_task->taskId);
        else
            hello = QStringLiteral("SEND_FILE %1\n").arg(m_task->taskId);

        if (!sendAll(&dataSock, hello.toUtf8().constData(), hello.toUtf8().size())) {
            emit taskError(m_task->taskId, QStringLiteral("\u63e1\u624b\u53d1\u9001\u5931\u8d25"));
            return;
        }

        // --- Open source file ---
        QFile inFile(m_task->fileName);
        if (!inFile.open(QIODevice::ReadOnly)) {
            emit taskError(m_task->taskId,
                           QStringLiteral("\u6253\u5f00\u6587\u4ef6\u5931\u8d25: ") + m_task->fileName);
            return;
        }

        const qint64 chunkSize = chunkSizeForMode(m_task->isLan);
        qint64 startOffset = (qint64)m_task->resumeChunk * chunkSize;
        inFile.seek(startOffset);

        QByteArray buffer(chunkSize, '\0');

        auto beginTime = std::chrono::steady_clock::now();
        auto lastReportTime = beginTime;
        qint64 sessionSentBytes = 0;
        qint64 reportBytes = 0;

        int chunkIndex = m_task->resumeChunk;
        bool failed = false;

        while (chunkIndex < m_task->totalChunks) {
            // --- Pause check ---
            {
                QMutexLocker lock(&m_task->mtx);
                while (m_task->paused && !m_task->finished)
                    m_task->cv.wait(&m_task->mtx);
                if (m_task->finished) break;
            }

            qint64 n = inFile.read(buffer.data(), chunkSize);
            if (n <= 0) break;

            ChunkHeader hdr{};
            hdr.task_id = m_task->taskId;
            hdr.chunk_index = chunkIndex;
            hdr.data_size = (qint32)n;

            if (!sendAll(&dataSock, (const char*)&hdr, sizeof(hdr))) {
                emit taskError(m_task->taskId, QStringLiteral("Send header failed"));
                failed = true;
                break;
            }
            if (!sendAll(&dataSock, buffer.constData(), hdr.data_size)) {
                emit taskError(m_task->taskId, QStringLiteral("Send data failed"));
                failed = true;
                break;
            }

            // --- Wait for ACK ---
            if (m_task->isLan) {
                char ack[4];
                if (!recvAll(&dataSock, ack, 4) || strncmp(ack, "ACK\n", 4) != 0) {
                    emit taskError(m_task->taskId, QStringLiteral("LAN ACK failed"));
                    failed = true;
                    break;
                }
                QMutexLocker lock(&m_task->mtx);
                m_task->lastAckedChunk = chunkIndex;
            } else {
                // ECS mode: wait for CHUNK_ACK from control channel
                QMutexLocker lock(&m_task->mtx);
                while (m_task->lastAckedChunk < chunkIndex && !m_task->finished)
                    m_task->cv.wait(&m_task->mtx);
                if (m_task->finished) {
                    failed = true;
                    break;
                }
            }

            sessionSentBytes += hdr.data_size;
            reportBytes += hdr.data_size;

            auto now = std::chrono::steady_clock::now();
            double interval = std::chrono::duration<double>(now - lastReportTime).count();
            if (interval >= 0.5) {
                double speed = calcMbps(reportBytes, interval);
                qint64 total = (qint64)(chunkIndex + 1) * chunkSize;
                if (total > m_task->fileSize) total = m_task->fileSize;
                emit taskProgress(m_task->taskId, total, speed);
                lastReportTime = now;
                reportBytes = 0;
            }

            ++chunkIndex;
        }

        if (failed) {
            dataSock.disconnectFromHost();
            if (dataSock.state() != QAbstractSocket::UnconnectedState)
                dataSock.waitForDisconnected(3000);
            return;
        }

        // --- Send end marker ---
        ChunkHeader endHdr{};
        endHdr.task_id = m_task->taskId;
        endHdr.chunk_index = chunkIndex;
        endHdr.data_size = 0;
        sendAll(&dataSock, (const char*)&endHdr, sizeof(endHdr));

        {
            QMutexLocker lock(&m_task->mtx);
            m_task->finished = true;
            m_task->cv.wakeAll();
        }

        inFile.close();
        dataSock.disconnectFromHost();
        if (dataSock.state() != QAbstractSocket::UnconnectedState)
            dataSock.waitForDisconnected(3000);

        if (!m_task->isLan)
            emit sendCtrlMessage(QStringLiteral("FILE_DONE %1\n").arg(m_task->taskId));

        emit taskFinished(m_task->taskId);
    }

private:
    QSharedPointer<SendTaskInfo> m_task;
    QString m_dataDir;
};

// ============================================================================
//  RecvWorkerThread  —  runs in its own QThread
//  For LAN: receives an existing connected socket (moved to this thread)
//  For ECS: creates its own connection to data_server
// ============================================================================

class RecvWorkerThread : public QThread {
    Q_OBJECT
public:
    RecvWorkerThread(int taskId, const QString& fileName, qint64 fileSize,
                     int resumeChunk, bool isLan, const QString& dataDir,
                     QSharedPointer<TransferControl> control,
                     QObject* parent = nullptr)
        : QThread(parent), m_taskId(taskId), m_fileName(fileName),
          m_fileSize(fileSize), m_resumeChunk(resumeChunk),
          m_isLan(isLan), m_dataDir(dataDir), m_control(control) {}

    void setLanSocket(QTcpSocket* sock) { m_lanSocket = sock; }

signals:
    void taskProgress(int taskId, qint64 bytesRecv, double speedMBps);
    void taskFinished(int taskId);
    void taskError(int taskId, const QString& msg);
    void sendCtrlMessage(const QString& msg);

protected:
    void run() override
    {
        QTcpSocket* dataSock = nullptr;
        bool ownSocket = false;

        if (m_isLan) {
            dataSock = m_lanSocket;  // already moved to this thread
        } else {
            dataSock = new QTcpSocket();
            tuneDataSocket(dataSock);
            ownSocket = true;
            dataSock->connectToHost(QStringLiteral(ECS_SERVER_IP), ECS_DATA_PORT);
            if (!dataSock->waitForConnected(10000)) {
                emit taskError(m_taskId,
                               QStringLiteral("\u6570\u636e\u8fde\u63a5\u5931\u8d25: ") + dataSock->errorString());
                delete dataSock;
                return;
            }
            QString hello = QStringLiteral("RECV_FILE %1\n").arg(m_taskId);
            if (!sendAll(dataSock, hello.toUtf8().constData(), hello.toUtf8().size())) {
                emit taskError(m_taskId, QStringLiteral("\u63e1\u624b\u53d1\u9001\u5931\u8d25"));
                delete dataSock;
                return;
            }
        }

        // --- Prepare output file ---
        QString recvDir = m_dataDir + QStringLiteral("/received_file");
        QString partDir = recvDir + QStringLiteral("/part");
        QDir().mkpath(partDir);

        m_fileName = safeFileNameToken(m_fileName);
        QString saveName = QDir(recvDir).absoluteFilePath(m_fileName);
        QString recvRoot = QDir(recvDir).absolutePath();
        QString saveAbsolute = QFileInfo(saveName).absoluteFilePath();
        recvRoot.replace('\\', '/');
        saveAbsolute.replace('\\', '/');
        if (!saveAbsolute.startsWith(recvRoot + QStringLiteral("/"))) {
            emit taskError(m_taskId, QStringLiteral("Unsafe output file name"));
            if (ownSocket) delete dataSock;
            return;
        }

        if (m_resumeChunk == 0) {
            // Fresh start: truncate
            QFile f(saveName);
            (void)f.open(QIODevice::WriteOnly | QIODevice::Truncate);
            f.close();
        } else {
            // Ensure file exists
            if (!QFile::exists(saveName)) {
                QFile f(saveName);
                (void)f.open(QIODevice::WriteOnly);
                f.close();
            }
        }

        QFile outFile(saveName);
        if (!outFile.open(QIODevice::ReadWrite)) {
            emit taskError(m_taskId, QStringLiteral("Open output failed: ") + saveName);
            if (ownSocket) delete dataSock;
            return;
        }

        const qint64 chunkSize = chunkSizeForMode(m_isLan);
        qint64 totalRecv = (qint64)m_resumeChunk * chunkSize;
        if (totalRecv > m_fileSize) totalRecv = m_fileSize;

        auto beginTime = std::chrono::steady_clock::now();
        auto lastReportTime = beginTime;
        qint64 sessionRecvBytes = 0;
        qint64 reportBytes = 0;
        bool gotEndMarker = false;
        bool failed = false;

        while (true) {
            if (!waitIfPaused())
                break;
            ChunkHeader hdr{};
            if (!recvAll(dataSock, (char*)&hdr, sizeof(hdr))) { failed = true; break; }
            if (hdr.task_id != m_taskId) { failed = true; break; }
            if (hdr.data_size < 0 || hdr.data_size > chunkSize) { failed = true; break; }
            if (hdr.data_size == 0) { gotEndMarker = true; break; }  // end marker
            if (hdr.chunk_index < m_resumeChunk || hdr.chunk_index * chunkSize > m_fileSize) {
                failed = true;
                break;
            }

            static constexpr int ResumeSaveIntervalChunks = 16;
            QByteArray buf(hdr.data_size, '\0');
            if (!recvAll(dataSock, buf.data(), hdr.data_size)) { failed = true; break; }

            qint64 offset = (qint64)hdr.chunk_index * chunkSize;
            outFile.seek(offset);
            outFile.write(buf.constData(), hdr.data_size);

            totalRecv += hdr.data_size;
            sessionRecvBytes += hdr.data_size;
            reportBytes += hdr.data_size;

            int nextChunk = hdr.chunk_index + 1;
            if ((nextChunk % ResumeSaveIntervalChunks) == 0 || totalRecv >= m_fileSize) {
                outFile.flush();
                QString resumePath = m_dataDir + "/received_file/part/" + m_fileName + ".resume";
                QFile rf(resumePath);
                if (rf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    const QByteArray fingerprint = QCryptographicHash::hash(
                        QStringLiteral("%1|%2|%3").arg(m_fileName).arg(m_fileSize).arg(chunkSize).toUtf8(),
                        QCryptographicHash::Sha256).toHex();
                    QJsonObject info;
                    info["version"] = 2;
                    info["fileName"] = m_fileName;
                    info["fileSize"] = QString::number(m_fileSize);
                    info["chunkSize"] = QString::number(chunkSize);
                    info["nextChunk"] = nextChunk;
                    info["fingerprint"] = QString::fromLatin1(fingerprint);
                    rf.write(QJsonDocument(info).toJson(QJsonDocument::Compact));
                    rf.close();
                }
            }

            if (!waitIfPaused())
                break;

            // Send ACK
            if (m_isLan) {
                sendAll(dataSock, "ACK\n", 4);
            } else {
                emit sendCtrlMessage(QStringLiteral("CHUNK_ACK %1 %2\n")
                                         .arg(m_taskId).arg(hdr.chunk_index));
            }

            auto now = std::chrono::steady_clock::now();
            double interval = std::chrono::duration<double>(now - lastReportTime).count();
            if (interval >= 0.5) {
                double speed = calcMbps(reportBytes, interval);
                emit taskProgress(m_taskId, totalRecv, speed);
                lastReportTime = now;
                reportBytes = 0;
            }
        }

        // Clean up resume info if complete
        if (totalRecv >= m_fileSize) {
            QString resumePath = m_dataDir + "/received_file/part/" + m_fileName + ".resume";
            QFile::remove(resumePath);
        }

        outFile.close();
        dataSock->disconnectFromHost();
        if (dataSock->state() != QAbstractSocket::UnconnectedState)
            dataSock->waitForDisconnected(3000);
        if (ownSocket) delete dataSock;

        const bool success = !failed && gotEndMarker && totalRecv >= m_fileSize;

        if (!m_isLan && success)
            emit sendCtrlMessage(QStringLiteral("FILE_DONE %1\n").arg(m_taskId));

        if (success)
            emit taskFinished(m_taskId);
        else
            emit taskError(m_taskId, QStringLiteral("Receive incomplete"));
    }

private:
    bool waitIfPaused()
    {
        if (!m_control)
            return !isInterruptionRequested();

        QMutexLocker lock(&m_control->mtx);
        while (m_control->paused && !m_control->finished && !isInterruptionRequested())
            m_control->cv.wait(&m_control->mtx, 250);
        return !m_control->finished && !isInterruptionRequested();
    }

    int m_taskId;
    QString m_fileName;
    qint64 m_fileSize;
    int m_resumeChunk;
    bool m_isLan;
    QString m_dataDir;
    QSharedPointer<TransferControl> m_control;
    QTcpSocket* m_lanSocket = nullptr;
};

// ============================================================================
//  NetworkManager — Constructor / Destructor
// ============================================================================

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_chatSettings(QStringLiteral("LanChatShell"), QStringLiteral("ChatHistory"))
{
    // Ensure receive directories
    QDir().mkpath(baseDataDir() + "/received_file/part");

    initLanServers();
}

NetworkManager::~NetworkManager()
{
    m_running = false;
    stopAllWorkers();

    if (m_ecsSocket) {
        m_ecsSocket->disconnectFromHost();
        delete m_ecsSocket;
    }
    if (m_udpSocket) delete m_udpSocket;
    if (m_lanCtrlServer) delete m_lanCtrlServer;
    if (m_lanDataServer) delete m_lanDataServer;
}

// ============================================================================
//  Property Getters
// ============================================================================

int NetworkManager::connectionMode() const { return m_connectionMode; }
QString NetworkManager::myName() const { return m_myName; }
bool NetworkManager::ecsConnected() const { return m_ecsConnected; }
bool NetworkManager::hasIncomingOffer() const { return m_incomingOffer.valid; }
QString NetworkManager::offerFrom() const { return m_incomingOffer.fromUser; }
QString NetworkManager::offerFileName() const { return m_incomingOffer.fileName; }
qint64 NetworkManager::offerFileSize() const { return m_incomingOffer.fileSize; }
bool NetworkManager::offerIsLan() const { return m_incomingOffer.isLan; }
QVariantList NetworkManager::chatMessages() const { return m_chatMessages; }

QVariantList NetworkManager::discoveredPeers() const
{
    QVariantList list;
    QMutexLocker lock(const_cast<QMutex*>(&m_peersMutex));
    for (auto it = m_discoveredPeers.constBegin(); it != m_discoveredPeers.constEnd(); ++it) {
        QVariantMap m;
        m["name"] = it.key();
        QString endpoint = it.value();
        int colon = endpoint.indexOf(':');
        m["ip"] = (colon > 0) ? endpoint.left(colon) : endpoint;
        m["port"] = (colon > 0) ? endpoint.mid(colon + 1).toInt() : LAN_CTRL_PORT;
        m["endpoint"] = endpoint;
        m["isLan"] = true;
        m["isSelf"] = false;
        m["displayName"] = looksLikeLegacyLanPeer(it.key()) ? QStringLiteral("局域网设备") : it.key();
        list.append(m);
    }
    return list;
}

QVariantList NetworkManager::onlineUsers() const
{
    QVariantList list;
    QVariantList others;
    for (const QString& name : m_onlineUsersList) {
        QVariantMap m;
        QString clean = name;
        bool present = clean.endsWith("(present)");
        if (present) clean.chop(9);
        m["name"] = clean;
        m["isPresent"] = present;
        m["isLan"] = false;
        bool self = isSelfName(clean) || present;
        m["isSelf"] = self;
        m["displayName"] = (self ? clean + QStringLiteral("（当前设备）") : clean);
        if (self)
            list.prepend(m);
        else
            others.append(m);
    }
    for (const QVariant& item : others)
        list.append(item);

    if (!m_myName.isEmpty() && m_connectionMode == EcsMode) {
        bool hasSelf = false;
        for (const QVariant& item : list) {
            if (item.toMap().value("isSelf").toBool()) {
                hasSelf = true;
                break;
            }
        }
        if (!hasSelf) {
            QVariantMap self;
            self["name"] = m_myName;
            self["displayName"] = m_myName + QStringLiteral("（当前设备）");
            self["isPresent"] = true;
            self["isLan"] = false;
            self["isSelf"] = true;
            list.prepend(self);
        }
    }
    return list;
}

QVariantList NetworkManager::transfers() const
{
    QVariantList list;
    QMutexLocker lock(const_cast<QMutex*>(&m_transferDisplayMutex));
    for (auto it = m_transferDisplay.constBegin(); it != m_transferDisplay.constEnd(); ++it) {
        const auto& d = it.value();
        QVariantMap m;
        m["taskId"] = d.taskId;
        m["fileName"] = d.fileName;
        m["fileSize"] = d.fileSize;
        m["bytesTransferred"] = d.bytesTransferred;
        m["progress"] = (d.fileSize > 0) ? (double)d.bytesTransferred / d.fileSize : 0.0;
        m["speedMBps"] = d.speedMBps;
        m["state"] = d.state;
        m["peerName"] = d.peerName;
        m["isLan"] = d.isLan;
        m["direction"] = d.direction;
        list.append(m);
    }
    return list;
}

// ============================================================================
//  Login / Mode
// ============================================================================

void NetworkManager::loginEcs(const QString& user, const QString& pass)
{
    if (m_ecsSocket) {
        m_ecsSocket->disconnectFromHost();
        m_ecsSocket->deleteLater();
        m_ecsSocket = nullptr;
    }

    m_pendingLoginUser = user;
    m_pendingLoginPass = pass;
    m_myName = user;
    m_onlineUsersList.clear();
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        m_transferDisplay.clear();
    }
    m_pendingRequests.clear();
    loadChatHistory();
    {
        QMutexLocker lock(&m_peersMutex);
        m_discoveredPeers.clear();
    }
    emit myNameChanged();
    emit receivedFilesDirChanged();
    QDir().mkpath(receivedFilesDir() + QStringLiteral("/part"));
    emit transfersChanged();
    emit onlineUsersChanged();
    emit peersChanged();

    m_ecsSocket = new QTcpSocket(this);
    tuneDataSocket(m_ecsSocket);
    connect(m_ecsSocket, &QTcpSocket::connected, this, &NetworkManager::onEcsConnected);
    connect(m_ecsSocket, &QTcpSocket::readyRead, this, &NetworkManager::onEcsReadyRead);
    connect(m_ecsSocket, &QTcpSocket::disconnected, this, &NetworkManager::onEcsDisconnected);
    connect(m_ecsSocket, &QTcpSocket::errorOccurred, this, &NetworkManager::onEcsError);

    m_ecsSocket->connectToHost(QStringLiteral(ECS_SERVER_IP), ECS_CTRL_PORT);
}

void NetworkManager::logout()
{
    if (m_ecsSocket) {
        m_ecsSocket->disconnectFromHost();
        m_ecsSocket->deleteLater();
        m_ecsSocket = nullptr;
    }
    m_ecsConnected = false;
    m_ecsPending.clear();
    m_connectionMode = Disconnected;
    m_onlineUsersList.clear();
    emit ecsConnectedChanged();
    emit connectionModeChanged();
    emit receivedFilesDirChanged();
    emit onlineUsersChanged();
}

void NetworkManager::clearLocalData()
{
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        m_transferDisplay.clear();
    }
    m_pendingRequests.clear();
    emit transfersChanged();

    m_chatMessages.clear();
    m_chatSettings.remove(chatHistoryKey());
    emit chatMessagesChanged();

    QDir().mkpath(receivedFilesDir());
    clearDirectoryContents(receivedFilesDir());
    QDir().mkpath(receivedFilesDir() + QStringLiteral("/part"));

    emit notification(QStringLiteral("本地记录已清空"));
}

void NetworkManager::clearAllLocalData()
{
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        m_transferDisplay.clear();
    }
    m_pendingRequests.clear();
    emit transfersChanged();

    m_chatMessages.clear();
    m_chatSettings.beginGroup(QStringLiteral("messages"));
    m_chatSettings.remove(QString());
    m_chatSettings.endGroup();
    m_chatSettings.remove(QStringLiteral("messages"));
    emit chatMessagesChanged();

    QDir root(appRootDir());
    QDir legacyReceived(root.absoluteFilePath(QStringLiteral("received_file")));
    if (legacyReceived.exists())
        clearDirectoryContents(legacyReceived.absolutePath());

    const QFileInfoList children = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& child : children) {
        QDir childReceived(QDir(child.absoluteFilePath()).absoluteFilePath(QStringLiteral("received_file")));
        if (childReceived.exists())
            clearDirectoryContents(childReceived.absolutePath());
    }

    QDir().mkpath(receivedFilesDir() + QStringLiteral("/part"));
    emit receivedFilesDirChanged();
    emit notification(QStringLiteral("所有本地记录已清空"));
}

void NetworkManager::startLanMode(const QString& displayName)
{
    m_myName = displayName;
    m_connectionMode = LanOnlyMode;
    m_onlineUsersList.clear();
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        m_transferDisplay.clear();
    }
    m_pendingRequests.clear();
    m_chatMessages.clear();
    {
        QMutexLocker lock(&m_peersMutex);
        m_discoveredPeers.clear();
    }
    emit myNameChanged();
    emit receivedFilesDirChanged();
    QDir().mkpath(receivedFilesDir() + QStringLiteral("/part"));
    emit chatMessagesChanged();
    emit transfersChanged();
    emit onlineUsersChanged();
    emit peersChanged();
    emit connectionModeChanged();
    QTimer::singleShot(0, this, [this]() { discover(false); });
}

// ============================================================================
//  ECS Control Socket Handlers
// ============================================================================

void NetworkManager::onEcsConnected()
{
    qDebug() << "[ECS] Connected, sending LOGIN";
    QString user = m_pendingLoginUser;
    QString pass = m_pendingLoginPass;
    user.remove(QRegularExpression(QStringLiteral("[\\r\\n]")));
    pass.remove(QRegularExpression(QStringLiteral("[\\r\\n]")));
    QString cmd = QStringLiteral("LOGIN %1 %2\n").arg(user, pass);
    m_ecsSocket->write(cmd.toUtf8());
}

void NetworkManager::onEcsReadyRead()
{
    if (!m_ecsSocket) return;
    QByteArray data = m_ecsSocket->readAll();
    m_ecsPending += QString::fromUtf8(data);

    while (true) {
        int pos = m_ecsPending.indexOf('\n');
        if (pos < 0) break;

        QString line = m_ecsPending.left(pos);
        m_ecsPending.remove(0, pos + 1);

        while (!line.isEmpty() && (line.back() == '\r' || line.back() == '\n'))
            line.chop(1);

        if (!line.isEmpty())
            handleServerLine(line);
    }
}

void NetworkManager::onEcsDisconnected()
{
    qDebug() << "[ECS] Disconnected";
    if (m_ecsConnected) {
        m_ecsConnected = false;
        m_connectionMode = Disconnected;
        emit ecsConnectedChanged();
        emit connectionModeChanged();
        emit notification(QStringLiteral("与 ECS 服务器断开连接"));
    }
}

void NetworkManager::onEcsError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (!m_ecsConnected) {
        emit loginResult(false, m_ecsSocket ? m_ecsSocket->errorString()
                                            : QStringLiteral("\u8fde\u63a5\u5931\u8d25"));
    }
}

void NetworkManager::sendCtrlText(const QString& text)
{
    if (m_ecsSocket && m_ecsSocket->state() == QAbstractSocket::ConnectedState)
        m_ecsSocket->write(text.toUtf8());
}

// ============================================================================
//  ECS Message Parsing  —  direct port of handle_server_line()
// ============================================================================

void NetworkManager::handleServerLine(const QString& line)
{
    qDebug() << "[ECS Recv]" << line;

    // ---------- LOGIN response ----------
    if (line == QStringLiteral("LOGIN_OK")) {
        m_ecsConnected = true;
        m_connectionMode = EcsMode;
        emit ecsConnectedChanged();
        emit connectionModeChanged();
        emit loginResult(true, QString());
        QTimer::singleShot(0, this, [this]() {
            ecsListUsers();
            discover(false);
        });
        return;
    }
    if (line.startsWith(QStringLiteral("ERROR "))) {
        QString err = line.mid(6);
        if (!m_ecsConnected) {
            emit loginResult(false, err);
            if (m_ecsSocket) { m_ecsSocket->disconnectFromHost(); }
        } else {
            emit notification(QStringLiteral("服务器错误: ") + err);
        }
        return;
    }

    // ---------- FILE_OFFER ----------
    if (line.startsWith(QStringLiteral("FILE_OFFER "))) {
        if (m_incomingOffer.valid) {
            emit notification(QStringLiteral("已有待处理的文件请求，新请求被忽略"));
            return;
        }
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 4) {
            const QString fromUser = decodeProtocolToken(parts[1]);
            const QString fileName = safeTransferFileName(decodeProtocolToken(parts[2]));
            const qint64 fileSize = parts[3].toLongLong();
            if (isSelfName(fromUser)) {
                int resumeChunk = 0;
                loadResumeInfo(fileName, fileSize, chunkSizeForMode(false), resumeChunk);
                sendCtrlText(QStringLiteral("YES %1\n").arg(resumeChunk));
                emit notification(QStringLiteral("正在自动接收自己的文件: %1").arg(fileName));
                return;
            }
            m_incomingOffer.valid = true;
            m_incomingOffer.isLan = false;
            m_incomingOffer.fromUser = fromUser;
            m_incomingOffer.fileName = fileName;
            m_incomingOffer.fileSize = fileSize;
            emit incomingOfferChanged();
        }
        return;
    }

    // ---------- FILE_ACCEPTED ----------
    if (line.startsWith(QStringLiteral("FILE_ACCEPTED "))) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        // FILE_ACCEPTED taskId peerName fileName fileSize resumeChunk
        if (parts.size() >= 6) {
            int taskId = parts[1].toInt();
            QString peerName = decodeProtocolToken(parts[2]);
            QString fileName = safeTransferFileName(decodeProtocolToken(parts[3]));
            qint64 fileSize = parts[4].toLongLong();
            int resumeChunk = parts[5].toInt();

            // Am I the sender?
            QString key = peerName + "|" + fileName;
            OutgoingFileReq pendingReq = m_pendingRequests.value(key);
            bool iAmSender = !pendingReq.fileName.isEmpty();

            if (iAmSender) {
                m_pendingRequests.remove(key);

                auto task = QSharedPointer<SendTaskInfo>::create();
                task->taskId = taskId;
                task->peerName = peerName;
                task->fileName = pendingReq.fileName;
                task->fileSize = fileSize;
                task->resumeChunk = resumeChunk;
                task->lastAckedChunk = resumeChunk - 1;
                const qint64 chunkSize = chunkSizeForMode(false);
                task->totalChunks = (int)((fileSize + chunkSize - 1) / chunkSize);
                task->isLan = false;

                {
                    QMutexLocker lock(&m_sendTasksMutex);
                    m_sendTasks[taskId] = task;
                }

                // Display
                TransferDisplayInfo di;
                di.taskId = taskId;
                di.fileName = fileName;
                di.fileSize = fileSize;
                di.peerName = peerName;
                di.state = "sending";
                di.direction = "send";
                di.isLan = false;
                updateTransferDisplay(taskId, di);

                startSendWorker(task);
                emit notification(QStringLiteral("开始发送 %1 (从 chunk %2 恢复)")
                                      .arg(fileName).arg(resumeChunk));
            } else {
                // I am the receiver
                TransferDisplayInfo di;
                di.taskId = taskId;
                di.fileName = fileName;
                di.fileSize = fileSize;
                di.peerName = peerName;
                di.state = "receiving";
                di.direction = "recv";
                di.isLan = false;
                updateTransferDisplay(taskId, di);

                startRecvWorker(taskId, fileName, fileSize, resumeChunk, nullptr, false);
                emit notification(QStringLiteral("开始接收 %1 (从 chunk %2 恢复)")
                                      .arg(fileName).arg(resumeChunk));
            }
        }
        return;
    }

    // ---------- CHUNK_ACK ----------
    if (line.startsWith(QStringLiteral("CHUNK_ACK "))) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            int taskId = parts[1].toInt();
            int chunkIndex = parts[2].toInt();

            QSharedPointer<SendTaskInfo> task;
            {
                QMutexLocker lock(&m_sendTasksMutex);
                task = m_sendTasks.value(taskId);
            }
            if (task) {
                QMutexLocker lock(&task->mtx);
                if (chunkIndex > task->lastAckedChunk)
                    task->lastAckedChunk = chunkIndex;
                task->cv.wakeAll();
            }
        }
        return;
    }

    // ---------- PAUSE ----------
    if (line.startsWith(QStringLiteral("PAUSE "))) {
        int taskId = line.mid(6).trimmed().toInt();
        pauseTask(taskId);
        emit notification(QStringLiteral("任务 %1 已暂停 (远端请求)").arg(taskId));
        return;
    }

    // ---------- RESUME ----------
    if (line.startsWith(QStringLiteral("RESUME "))) {
        int taskId = line.mid(7).trimmed().toInt();
        resumeTask(taskId);
        emit notification(QStringLiteral("任务 %1 已恢复 (远端请求)").arg(taskId));
        return;
    }

    // ---------- ONLINE (user list response) ----------
    if (line.startsWith(QStringLiteral("ONLINE"))) {
        QStringList users;
        for (const QString& item : line.mid(7).split(' ', Qt::SkipEmptyParts))
            users.append(decodeProtocolToken(item));
        m_onlineUsersList = users;
        emit onlineUsersChanged();
        return;
    }

    // ---------- FROM (chat message) ----------
    if (line.startsWith(QStringLiteral("FROM "))) {
        QStringList parts = line.split(' ');
        if (parts.size() >= 3) {
            QString from = decodeProtocolToken(parts[1]);
            if (isSelfName(from))
                return;
            QString content = decodeProtocolToken(QStringList(parts.mid(2)).join(' '));
            appendChatMessage(from, m_myName, content, false);
            emit notification(QStringLiteral("来自 %1 的消息: %2").arg(from, content));
        }
        return;
    }

    // ---------- FILE_DONE ----------
    if (line.startsWith(QStringLiteral("FILE_DONE "))) {
        int taskId = line.mid(10).trimmed().toInt();
        {
            QMutexLocker lock(&m_transferDisplayMutex);
            if (m_transferDisplay.contains(taskId)) {
                auto& d = m_transferDisplay[taskId];
                d.state = "completed";
                d.bytesTransferred = d.fileSize;
            }
        }
        emit transfersChanged();
        emit notification(QStringLiteral("任务 %1 已完成").arg(taskId));
        return;
    }

    // ---------- Other server messages ----------
    emit notification(QStringLiteral("[\u670d\u52a1\u5668] ") + line);
}

// ============================================================================
//  LAN Discovery
// ============================================================================

void NetworkManager::initLanServers()
{
    // UDP discovery socket
    m_udpSocket = new QUdpSocket(this);
    m_udpSocket->bind(QHostAddress::AnyIPv4, LAN_DISCOVERY_PORT,
                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    // LAN control server (port 9002)
    m_lanCtrlServer = new QTcpServer(this);
    if (m_lanCtrlServer->listen(QHostAddress::AnyIPv4, LAN_CTRL_PORT)) {
        m_myLanCtrlPort = LAN_CTRL_PORT;
        connect(m_lanCtrlServer, &QTcpServer::newConnection,
                this, &NetworkManager::onLanCtrlNewConnection);
    } else {
        qWarning() << "[LAN] Ctrl server listen failed on" << LAN_CTRL_PORT;
    }

    // LAN data server (port 9000)
    m_lanDataServer = new QTcpServer(this);
    if (m_lanDataServer->listen(QHostAddress::AnyIPv4, LAN_DATA_PORT)) {
        m_myLanDataPort = LAN_DATA_PORT;
        connect(m_lanDataServer, &QTcpServer::newConnection,
                this, &NetworkManager::onLanDataNewConnection);
    } else {
        qWarning() << "[LAN] Data server listen failed on" << LAN_DATA_PORT;
    }
}

void NetworkManager::discover(bool showNotification)
{
    if (m_myName.isEmpty()) {
        if (showNotification)
            emit notification(QStringLiteral("请先设置用户名"));
        return;
    }

    QByteArray req = QStringLiteral("LAN_TRANSFER_DISCOVER_V2|%1|%2")
                         .arg(encodeProtocolToken(m_myName)).arg(m_myLanCtrlPort).toUtf8();

    // Broadcast on 255.255.255.255
    m_udpSocket->writeDatagram(req, QHostAddress::Broadcast, LAN_DISCOVERY_PORT);

    // Also broadcast on each interface's subnet broadcast address
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp) ||
            iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QHostAddress bc = entry.broadcast();
                if (!bc.isNull() && bc != QHostAddress::Broadcast)
                    m_udpSocket->writeDatagram(req, bc, LAN_DISCOVERY_PORT);
            }
        }
    }

    if (showNotification)
        emit notification(QStringLiteral("LAN 发现广播已发送..."));
}

void NetworkManager::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress from;
        quint16 fromPort;
        m_udpSocket->readDatagram(buf.data(), buf.size(), &from, &fromPort);

        QString msg = QString::fromUtf8(buf);
        QString ip = from.toString();
        // Remove IPv6 prefix if present (e.g. "::ffff:192.168.1.5")
        if (ip.startsWith("::ffff:"))
            ip = ip.mid(7);

        if (msg.startsWith(QStringLiteral("LAN_TRANSFER_DISCOVER"))) {
            // Incoming discovery request — reply with our info
            int firstBar = msg.indexOf('|');
            if (firstBar >= 0) {
                QString payload = msg.mid(firstBar + 1);
                int secondBar = payload.indexOf('|');
                if (secondBar >= 0) {
                    QString peerName = decodeProtocolToken(payload.left(secondBar));
                    // QString peerPort = payload.mid(secondBar + 1); // their ctrl port

                    if (peerName != m_myName) {
                        // Reply
                        QByteArray resp = QStringLiteral("LAN_TRANSFER_DEVICE_V2|%1|%2")
                                              .arg(encodeProtocolToken(m_myName)).arg(m_myLanCtrlPort).toUtf8();
                        m_udpSocket->writeDatagram(resp, from, LAN_DISCOVERY_PORT);
                    }
                }
            }
        }
        else if (msg.startsWith(QStringLiteral("LAN_TRANSFER_DEVICE_V"))) {
            // Discovery reply
            // Format: LAN_TRANSFER_DEVICE_V2|peerName|peerCtrlPort
            int bar1 = msg.indexOf('|');
            if (bar1 < 0) continue;
            QString rest = msg.mid(bar1 + 1);
            int bar2 = rest.indexOf('|');
            QString peerName, peerPort;
            if (bar2 >= 0) {
                peerName = decodeProtocolToken(rest.left(bar2));
                peerPort = rest.mid(bar2 + 1);
            } else {
                peerName = decodeProtocolToken(rest);
                peerPort = QString::number(LAN_CTRL_PORT);
            }

            if (peerName == m_myName) continue;

            QString endpoint = ip + ":" + peerPort;
            bool changed = false;
            {
                QMutexLocker lock(&m_peersMutex);
                if (m_discoveredPeers.value(peerName) != endpoint) {
                    m_discoveredPeers[peerName] = endpoint;
                    changed = true;
                }
            }
            if (changed) {
                emit peersChanged();
                emit notification(QStringLiteral("发现设备: %1 -> %2").arg(endpoint, peerName));
            }
        }
    }
}

// ============================================================================
//  LAN Control Server  —  handles incoming LAN_FILE_SEND
// ============================================================================

void NetworkManager::onLanCtrlNewConnection()
{
    while (m_lanCtrlServer->hasPendingConnections()) {
        QTcpSocket* client = m_lanCtrlServer->nextPendingConnection();
        tuneDataSocket(client);

        // Wait briefly for the request data
        connect(client, &QTcpSocket::readyRead, this, [this, client]() {
            QByteArray data = client->readAll();
            QString req = QString::fromUtf8(data).trimmed();

            if (req.startsWith(QStringLiteral("LAN_FILE_SEND "))) {
                QStringList parts = req.split(' ', Qt::SkipEmptyParts);
                // LAN_FILE_SEND fromUser fileName fileSize taskId
                if (parts.size() >= 5) {
                    QString fromUser = decodeProtocolToken(parts[1]);
                    QString fileName = safeTransferFileName(decodeProtocolToken(parts[2]));
                    qint64 fileSize = parts[3].toLongLong();
                    int taskId = parts[4].toInt();

                    QString peerIp = client->peerAddress().toString();
                    if (peerIp.startsWith("::ffff:")) peerIp = peerIp.mid(7);

                    if (isSelfName(fromUser) || peerIp == QStringLiteral("127.0.0.1")) {
                        int resumeChunk = 0;
                        loadResumeInfo(fileName, fileSize, chunkSizeForMode(true), resumeChunk);
                        QString resp = QStringLiteral("ACCEPT %1 %2\n").arg(resumeChunk).arg(m_myLanDataPort);
                        client->write(resp.toUtf8());
                        client->waitForBytesWritten(1000);
                        client->disconnectFromHost();
                        client->deleteLater();

                        {
                            QMutexLocker lock(&m_lanAcceptedMutex);
                            m_lanAcceptedTasks[taskId] = { fileName, fileSize, resumeChunk };
                        }
                        emit notification(QStringLiteral("正在自动接收自己的文件: %1").arg(fileName));
                        return;
                    }

                    if (m_incomingOffer.valid) {
                        client->write("REJECT\n");
                        client->waitForBytesWritten(3000);
                        client->disconnectFromHost();
                        client->deleteLater();
                        return;
                    }

                    m_incomingOffer.valid = true;
                    m_incomingOffer.isLan = true;
                    m_incomingOffer.lanSocket = client;
                    m_incomingOffer.taskId = taskId;
                    m_incomingOffer.fromUser = fromUser;
                    m_incomingOffer.fileName = fileName;
                    m_incomingOffer.fileSize = fileSize;
                    emit incomingOfferChanged();
                    // Don't close the socket yet — we reply ACCEPT/REJECT later
                }
            } else {
                client->disconnectFromHost();
                client->deleteLater();
            }
        });
    }
}

// ============================================================================
//  LAN Data Server  —  handles incoming LAN_DATA_HANDSHAKE
// ============================================================================

void NetworkManager::onLanDataNewConnection()
{
    while (m_lanDataServer->hasPendingConnections()) {
        QTcpSocket* client = m_lanDataServer->nextPendingConnection();
        tuneDataSocket(client);

        connect(client, &QTcpSocket::readyRead, this, [this, client]() {
            if (!client->canReadLine())
                return;

            // Read only the text handshake line. readAll() can consume the first
            // binary chunk if the sender starts immediately after the handshake.
            QByteArray data = client->readLine();
            disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);
            QString req = QString::fromUtf8(data).trimmed();

            if (req.startsWith(QStringLiteral("LAN_DATA_HANDSHAKE "))) {
                int taskId = req.mid(19).trimmed().toInt();

                LanAcceptedTask task;
                bool found = false;
                {
                    QMutexLocker lock(&m_lanAcceptedMutex);
                    auto it = m_lanAcceptedTasks.find(taskId);
                    if (it != m_lanAcceptedTasks.end()) {
                        task = it.value();
                        m_lanAcceptedTasks.erase(it);
                        found = true;
                    }
                }

                if (found) {
                    TransferDisplayInfo di;
                    di.taskId = taskId;
                    di.fileName = task.fileName;
                    di.fileSize = task.fileSize;
                    di.state = "receiving";
                    di.direction = "recv";
                    di.isLan = true;
                    di.peerName = client->peerAddress().toString();
                    updateTransferDisplay(taskId, di);

                    startRecvWorker(taskId, task.fileName, task.fileSize,
                                    task.resumeChunk, client, true);
                } else {
                    client->disconnectFromHost();
                    client->deleteLater();
                }
            } else {
                client->disconnectFromHost();
                client->deleteLater();
            }
        });
    }
}

// ============================================================================
//  LAN Send File  —  async handshake with peer's control port
// ============================================================================

void NetworkManager::lanSendFile(const QString& peerNameOrIp, const QString& filePath)
{
    if (isSelfName(peerNameOrIp)) {
        QString savedFileName;
        if (copyFileToReceivedDir(filePath, &savedFileName)) {
            QFileInfo sentFile(localPathFromUrl(filePath));
            appendFileChatMessage(m_myName, savedFileName, sentFile.size(), true, m_connectionMode == LanOnlyMode);
            emit transferCompleted(0, savedFileName);
            emit notification(QStringLiteral("已保存到文件库: %1").arg(savedFileName));
        }
        return;
    }

    QString ip;
    int targetPort = LAN_CTRL_PORT;

    if (peerNameOrIp.contains('.')) {
        ip = peerNameOrIp;
    } else {
        QMutexLocker lock(&m_peersMutex);
        if (m_discoveredPeers.contains(peerNameOrIp)) {
            QString ep = m_discoveredPeers[peerNameOrIp];
            int colon = ep.indexOf(':');
            if (colon > 0) {
                ip = ep.left(colon);
                targetPort = ep.mid(colon + 1).toInt();
            }
        } else {
            emit notification(QStringLiteral("未找到设备 '%1'，请先执行 LAN 发现").arg(peerNameOrIp));
            return;
        }
    }

    const QString localFilePath = localPathFromUrl(filePath);
    QFileInfo fi(localFilePath);
    if (!fi.exists() || !fi.isFile()) {
        emit notification(QStringLiteral("文件不存在: ") + localFilePath);
        return;
    }

    QString fileName = safeTransferFileName(fi.fileName());
    qint64 fileSize = fi.size();
    QString fullPath = fi.absoluteFilePath();
    int taskId = m_lanLocalTaskId.fetchAndAddRelaxed(1);

    QTcpSocket* handshakeSock = new QTcpSocket(this);
    tuneDataSocket(handshakeSock);

    connect(handshakeSock, &QTcpSocket::connected, this, [=]() {
        QString hello = QStringLiteral("LAN_FILE_SEND %1 %2 %3 %4\n")
                            .arg(encodeProtocolToken(m_myName), encodeProtocolToken(fileName))
                            .arg(fileSize).arg(taskId);
        handshakeSock->write(hello.toUtf8());
        emit notification(QStringLiteral("等待对端响应..."));
    });

    connect(handshakeSock, &QTcpSocket::readyRead, this, [=]() {
        QByteArray data = handshakeSock->readAll();
        QString resp = QString::fromUtf8(data).trimmed();

        if (resp.startsWith(QStringLiteral("ACCEPT "))) {
            QStringList parts = resp.split(' ');
            int resumeChunk = (parts.size() >= 2) ? parts[1].toInt() : 0;
            int peerDataPort = (parts.size() >= 3) ? parts[2].toInt() : LAN_DATA_PORT;

            auto task = QSharedPointer<SendTaskInfo>::create();
            task->taskId = taskId;
            task->peerName = ip;
            task->fileName = fullPath;
            task->fileSize = fileSize;
            task->resumeChunk = resumeChunk;
            task->lastAckedChunk = resumeChunk - 1;
            const qint64 chunkSize = chunkSizeForMode(true);
            task->totalChunks = (int)((fileSize + chunkSize - 1) / chunkSize);
            task->isLan = true;
            task->lanPeerIp = ip;
            task->lanDataPort = peerDataPort;

            {
                QMutexLocker lock(&m_sendTasksMutex);
                m_sendTasks[taskId] = task;
            }

            TransferDisplayInfo di;
            di.taskId = taskId;
            di.fileName = fileName;
            di.fileSize = fileSize;
            di.peerName = ip;
            di.state = "sending";
            di.direction = "send";
            di.isLan = true;
            updateTransferDisplay(taskId, di);

            appendFileChatMessage(peerNameOrIp, fileName, fileSize, true, true);
            startSendWorker(task);
            emit notification(QStringLiteral("对端已接受，开始发送 %1 (chunk %2)")
                                  .arg(fileName).arg(resumeChunk));
        } else {
            emit notification(QStringLiteral("对端拒绝了文件传输"));
        }

        handshakeSock->disconnectFromHost();
        handshakeSock->deleteLater();
    });

    connect(handshakeSock, &QTcpSocket::errorOccurred, this, [=]() {
        emit notification(QStringLiteral("连接 LAN 对端失败: ") + handshakeSock->errorString());
        handshakeSock->deleteLater();
    });

    handshakeSock->connectToHost(ip, targetPort);
}

// ============================================================================
//  ECS File Send / Message
// ============================================================================

void NetworkManager::ecsSendFile(const QString& toUser, const QString& filePath)
{
    if (isSelfName(toUser)) {
        QString savedFileName;
        if (copyFileToReceivedDir(filePath, &savedFileName)) {
            QFileInfo sentFile(localPathFromUrl(filePath));
            appendFileChatMessage(m_myName, savedFileName, sentFile.size(), true);
            emit transferCompleted(0, savedFileName);
            emit notification(QStringLiteral("已保存到文件库: %1").arg(savedFileName));
        }
        return;
    }

    if (!m_ecsConnected) {
        emit notification(QStringLiteral("未连接 ECS，无法发送"));
        return;
    }

    const QString localFilePath = localPathFromUrl(filePath);
    QFileInfo fi(localFilePath);
    if (!fi.exists() || !fi.isFile()) {
        emit notification(QStringLiteral("文件不存在: ") + localFilePath);
        return;
    }

    QString fileName = safeTransferFileName(fi.fileName());
    qint64 fileSize = fi.size();

    QString key = toUser + "|" + fileName;
    m_pendingRequests[key] = { toUser, fi.absoluteFilePath(), fileSize };

    sendCtrlText(QStringLiteral("FILE_SEND %1 %2 %3\n")
                     .arg(toUser, encodeProtocolToken(fileName))
                     .arg(fileSize));
    appendFileChatMessage(toUser, fileName, fileSize, true);
    emit notification(QStringLiteral("文件请求已发送给 %1").arg(toUser));
}

void NetworkManager::ecsSendMessage(const QString& toUser, const QString& message)
{
    if (isSelfName(toUser)) {
        QVariantMap extra;
        if (m_connectionMode == LanOnlyMode)
            extra["isLan"] = true;
        appendChatMessage(m_myName, m_myName, message, true, extra);
        return;
    }

    if (!m_ecsConnected || !isUserOnline(toUser)) {
        emit notification(QStringLiteral("%1 不在线 (offline)").arg(toUser));
        return;
    }
    QString safeMessage = message;
    safeMessage.remove(QRegularExpression(QStringLiteral("[\\r\\n]")));
    sendCtrlText(QStringLiteral("SEND %1 %2\n").arg(toUser, encodeProtocolToken(safeMessage)));

    appendChatMessage(m_myName, toUser, safeMessage, true);
}

void NetworkManager::ecsListUsers()
{
    if (!m_ecsConnected) {
        emit notification(QStringLiteral("未连接 ECS"));
        return;
    }
    sendCtrlText(QStringLiteral("LIST\n"));
}

void NetworkManager::ensureConversation(const QString& peerName)
{
    QString clean = peerName.trimmed();
    if (clean.isEmpty())
        return;

    for (const QVariant& item : m_chatMessages) {
        const QVariantMap msg = item.toMap();
        const QString peer = msg.value(QStringLiteral("isMe")).toBool()
                                 ? msg.value(QStringLiteral("to")).toString()
                                 : msg.value(QStringLiteral("from")).toString();
        if (peer == clean)
            return;
    }

    QVariantMap msg;
    msg["from"] = clean;
    msg["to"] = m_myName;
    msg["text"] = QString();
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    msg["isMe"] = false;
    msg["isPlaceholder"] = true;
    bool isLanConversation = (m_connectionMode == LanOnlyMode || isSelfName(clean));
    {
        QMutexLocker lock(&m_peersMutex);
        if (m_discoveredPeers.contains(clean))
            isLanConversation = true;
    }
    if (isLanConversation)
        msg["isLan"] = true;
    m_chatMessages.append(msg);
    saveChatHistory();
    emit chatMessagesChanged();
}

QString NetworkManager::localPathFromUrl(const QString& urlOrPath) const
{
    QUrl url(urlOrPath);
    if (url.isLocalFile())
        return QDir::toNativeSeparators(url.toLocalFile());
    if (url.scheme().isEmpty())
        return QDir::toNativeSeparators(urlOrPath);
    return urlOrPath;
}

QString NetworkManager::desktopDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    return dir;
}

// ============================================================================
//  Accept / Reject Offer
// ============================================================================

void NetworkManager::acceptOffer()
{
    if (!m_incomingOffer.valid) return;

    int resumeChunk = 0;
    loadResumeInfo(m_incomingOffer.fileName, m_incomingOffer.fileSize,
                   chunkSizeForMode(m_incomingOffer.isLan), resumeChunk);
    appendFileChatMessage(m_incomingOffer.fromUser, m_incomingOffer.fileName,
                          m_incomingOffer.fileSize, false, m_incomingOffer.isLan);

    if (m_incomingOffer.isLan) {
        QString resp = QStringLiteral("ACCEPT %1 %2\n").arg(resumeChunk).arg(m_myLanDataPort);
        m_incomingOffer.lanSocket->write(resp.toUtf8());
        m_incomingOffer.lanSocket->waitForBytesWritten(5000);
        // Don't delete the socket yet — it belongs to the ctrl channel
        m_incomingOffer.lanSocket->disconnectFromHost();
        m_incomingOffer.lanSocket->deleteLater();

        {
            QMutexLocker lock(&m_lanAcceptedMutex);
            m_lanAcceptedTasks[m_incomingOffer.taskId] = {
                m_incomingOffer.fileName,
                m_incomingOffer.fileSize,
                resumeChunk
            };
        }

        emit notification(QStringLiteral("LAN 已接受，等待数据连接..."));
    } else {
        sendCtrlText(QStringLiteral("YES %1\n").arg(resumeChunk));
    }

    m_incomingOffer.valid = false;
    emit incomingOfferChanged();
}

void NetworkManager::rejectOffer()
{
    if (!m_incomingOffer.valid) return;

    if (m_incomingOffer.isLan) {
        m_incomingOffer.lanSocket->write("REJECT\n");
        m_incomingOffer.lanSocket->waitForBytesWritten(3000);
        m_incomingOffer.lanSocket->disconnectFromHost();
        m_incomingOffer.lanSocket->deleteLater();
    } else {
        sendCtrlText(QStringLiteral("NO\n"));
    }

    m_incomingOffer.valid = false;
    emit incomingOfferChanged();
}

// ============================================================================
//  Pause / Resume
// ============================================================================

void NetworkManager::pauseTask(int taskId)
{
    QSharedPointer<SendTaskInfo> task;
    {
        QMutexLocker lock(&m_sendTasksMutex);
        task = m_sendTasks.value(taskId);
    }
    if (task) {
        QMutexLocker lock(&task->mtx);
        task->paused = true;
    }
    QSharedPointer<TransferControl> control = m_transferControls.value(taskId);
    if (control) {
        QMutexLocker lock(&control->mtx);
        control->paused = true;
    }
    // Update display
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        if (m_transferDisplay.contains(taskId))
            m_transferDisplay[taskId].state = "paused";
    }
    emit transfersChanged();
}

void NetworkManager::resumeTask(int taskId)
{
    QSharedPointer<SendTaskInfo> task;
    {
        QMutexLocker lock(&m_sendTasksMutex);
        task = m_sendTasks.value(taskId);
    }
    if (task) {
        QMutexLocker lock(&task->mtx);
        task->paused = false;
        task->cv.wakeAll();
    }
    QSharedPointer<TransferControl> control = m_transferControls.value(taskId);
    if (control) {
        QMutexLocker lock(&control->mtx);
        control->paused = false;
        control->cv.wakeAll();
    }
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        if (m_transferDisplay.contains(taskId))
            m_transferDisplay[taskId].state = m_transferDisplay[taskId].direction == "send"
                                                  ? "sending" : "receiving";
    }
    emit transfersChanged();
}

void NetworkManager::pauseAll()
{
    QSet<int> ids;
    {
        QMutexLocker lock(&m_sendTasksMutex);
        for (auto it = m_sendTasks.constBegin(); it != m_sendTasks.constEnd(); ++it) {
            if (!it.value()->finished) ids.insert(it.key());
        }
    }
    for (auto it = m_transferControls.constBegin(); it != m_transferControls.constEnd(); ++it)
        ids.insert(it.key());
    for (int id : ids) {
        pauseTask(id);
        // Notify peer in ECS mode
        QSharedPointer<SendTaskInfo> t;
        { QMutexLocker lock(&m_sendTasksMutex); t = m_sendTasks.value(id); }
        if (t && !t->isLan)
            sendCtrlText(QStringLiteral("PAUSE %1\n").arg(id));
    }
}

void NetworkManager::resumeAll()
{
    QSet<int> ids;
    {
        QMutexLocker lock(&m_sendTasksMutex);
        for (auto it = m_sendTasks.constBegin(); it != m_sendTasks.constEnd(); ++it) {
            if (!it.value()->finished) ids.insert(it.key());
        }
    }
    for (auto it = m_transferControls.constBegin(); it != m_transferControls.constEnd(); ++it)
        ids.insert(it.key());
    for (int id : ids) {
        resumeTask(id);
        QSharedPointer<SendTaskInfo> t;
        { QMutexLocker lock(&m_sendTasksMutex); t = m_sendTasks.value(id); }
        if (t && !t->isLan)
            sendCtrlText(QStringLiteral("RESUME %1\n").arg(id));
    }
}

// ============================================================================
//  Worker Thread Management
// ============================================================================

void NetworkManager::startSendWorker(QSharedPointer<SendTaskInfo> task)
{
    auto* worker = new SendWorkerThread(task, baseDataDir());
    m_workerThreads[task->taskId] = worker;

    connect(worker, &SendWorkerThread::taskProgress,
            this, &NetworkManager::onWorkerProgress, Qt::QueuedConnection);
    connect(worker, &SendWorkerThread::taskFinished,
            this, &NetworkManager::onWorkerFinished, Qt::QueuedConnection);
    connect(worker, &SendWorkerThread::taskError,
            this, &NetworkManager::onWorkerError, Qt::QueuedConnection);
    connect(worker, &SendWorkerThread::sendCtrlMessage,
            this, &NetworkManager::onWorkerSendCtrl, Qt::QueuedConnection);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    worker->start();
}

void NetworkManager::startRecvWorker(int taskId, const QString& fileName, qint64 fileSize,
                                     int resumeChunk, QTcpSocket* lanDataSocket, bool isLan)
{
    auto control = QSharedPointer<TransferControl>::create();
    m_transferControls[taskId] = control;
    auto* worker = new RecvWorkerThread(taskId, fileName, fileSize, resumeChunk,
                                        isLan, baseDataDir(), control);
    m_workerThreads[taskId] = worker;

    if (isLan && lanDataSocket) {
        // Move the socket from main thread to worker thread
        lanDataSocket->setParent(nullptr);
        disconnect(lanDataSocket, nullptr, nullptr, nullptr);
        lanDataSocket->moveToThread(worker);
        worker->setLanSocket(lanDataSocket);
    }

    connect(worker, &RecvWorkerThread::taskProgress,
            this, &NetworkManager::onWorkerProgress, Qt::QueuedConnection);
    connect(worker, &RecvWorkerThread::taskFinished,
            this, &NetworkManager::onWorkerFinished, Qt::QueuedConnection);
    connect(worker, &RecvWorkerThread::taskError,
            this, &NetworkManager::onWorkerError, Qt::QueuedConnection);
    connect(worker, &RecvWorkerThread::sendCtrlMessage,
            this, &NetworkManager::onWorkerSendCtrl, Qt::QueuedConnection);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    worker->start();
}

// ============================================================================
//  Worker Callbacks
// ============================================================================

void NetworkManager::onWorkerProgress(int taskId, qint64 bytesTransferred, double speedMBps)
{
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        if (m_transferDisplay.contains(taskId)) {
            m_transferDisplay[taskId].bytesTransferred = bytesTransferred;
            m_transferDisplay[taskId].speedMBps = speedMBps;
        }
    }
    emit transfersChanged();
}

void NetworkManager::onWorkerFinished(int taskId)
{
    markTransferFinished(taskId);
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        if (m_transferDisplay.contains(taskId)) {
            auto& d = m_transferDisplay[taskId];
            d.state = "completed";
            d.bytesTransferred = d.fileSize;
        }
    }
    m_workerThreads.remove(taskId);
    m_transferControls.remove(taskId);
    {
        QMutexLocker lock(&m_sendTasksMutex);
        m_sendTasks.remove(taskId);
    }
    emit transfersChanged();

    QString fileName;
    { QMutexLocker lock(&m_transferDisplayMutex); fileName = m_transferDisplay.value(taskId).fileName; }
    emit transferCompleted(taskId, fileName);
    emit notification(QStringLiteral("传输完成: %1").arg(fileName));
}

void NetworkManager::onWorkerError(int taskId, const QString& message)
{
    markTransferFinished(taskId);
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        if (m_transferDisplay.contains(taskId))
            m_transferDisplay[taskId].state = "failed";
    }
    m_workerThreads.remove(taskId);
    m_transferControls.remove(taskId);
    {
        QMutexLocker lock(&m_sendTasksMutex);
        m_sendTasks.remove(taskId);
    }
    emit transfersChanged();
    emit transferFailed(taskId, message);
    emit notification(QStringLiteral("传输失败 [%1]: %2").arg(taskId).arg(message));
}

void NetworkManager::onWorkerSendCtrl(const QString& message)
{
    sendCtrlText(message);
}

void NetworkManager::stopAllWorkers()
{
    for (auto it = m_sendTasks.begin(); it != m_sendTasks.end(); ++it) {
        QSharedPointer<SendTaskInfo> task = it.value();
        if (!task)
            continue;
        QMutexLocker lock(&task->mtx);
        task->finished = true;
        task->cv.wakeAll();
    }
    for (auto it = m_transferControls.begin(); it != m_transferControls.end(); ++it) {
        QSharedPointer<TransferControl> control = it.value();
        if (!control)
            continue;
        QMutexLocker lock(&control->mtx);
        control->finished = true;
        control->cv.wakeAll();
    }
    for (auto* t : m_workerThreads) {
        if (!t)
            continue;
        t->requestInterruption();
        t->quit();
    }
    for (auto* t : m_workerThreads) {
        if (t)
            t->wait(5000);
    }
    m_workerThreads.clear();
    m_transferControls.clear();
    m_sendTasks.clear();
}

// ============================================================================
//  File / Resume Helpers
// ============================================================================

QString NetworkManager::appRootDir() const
{
#ifdef Q_OS_ANDROID
    QString androidDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!androidDir.isEmpty())
        return androidDir;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#else
    auto findProjectRoot = [](const QString& startPath) -> QString {
        QDir dir(startPath);
        for (int i = 0; i < 8; ++i) {
            if (QFileInfo::exists(dir.absoluteFilePath(QStringLiteral("CMakeLists.txt"))) &&
                QDir(dir.absoluteFilePath(QStringLiteral("qml"))).exists()) {
                return dir.absolutePath();
            }
            if (!dir.cdUp())
                break;
        }
        return QString();
    };

    QString dir = findProjectRoot(QDir::currentPath());
    if (dir.isEmpty())
        dir = findProjectRoot(QCoreApplication::applicationDirPath());
    if (dir.isEmpty())
        dir = QCoreApplication::applicationDirPath();
    return dir;
#endif
}

QString NetworkManager::accountFolderName() const
{
    QString account = (m_connectionMode == LanOnlyMode || m_myName.isEmpty())
                          ? QStringLiteral("temp_user")
                          : m_myName.trimmed();
    account.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\s]+")), QStringLiteral("_"));
    account = account.trimmed();
    if (account.isEmpty())
        account = QStringLiteral("temp_user");
    return account;
}

QString NetworkManager::chatHistoryKey() const
{
    return QStringLiteral("messages/%1").arg(accountFolderName());
}

QString NetworkManager::baseDataDir() const
{
    return appRootDir() + "/" + accountFolderName();
}

QString NetworkManager::receivedFilesDir() const
{
    return baseDataDir() + "/received_file";
}

QString NetworkManager::outputFileNameFor(const QString& fileName) const
{
    QDir dir(baseDataDir() + "/received_file");
    return dir.absoluteFilePath(safeTransferFileName(fileName));
}

QString NetworkManager::resumeFileNameFor(const QString& fileName) const
{
    QDir dir(baseDataDir() + "/received_file/part");
    return dir.absoluteFilePath(safeTransferFileName(fileName) + ".resume");
}

QString NetworkManager::safeTransferFileName(const QString& fileName) const
{
    return safeFileNameToken(fileName);
}

QString NetworkManager::transferFingerprint(const QString& fileName, qint64 fileSize, qint64 chunkSize) const
{
    const QByteArray raw = QStringLiteral("%1|%2|%3")
                               .arg(safeTransferFileName(fileName))
                               .arg(fileSize)
                               .arg(chunkSize)
                               .toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex());
}

bool NetworkManager::loadResumeInfo(const QString& fileName, qint64 expectedSize,
                                    qint64 chunkSize, int& resumeChunk)
{
    resumeChunk = 0;
    const QString safeName = safeTransferFileName(fileName);
    QFile f(resumeFileNameFor(safeName));
    if (!f.open(QIODevice::ReadOnly)) return false;

    QString content = QString::fromUtf8(f.readAll()).trimmed();
    f.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();
        const qint64 savedSize = obj.value(QStringLiteral("fileSize")).toVariant().toLongLong();
        const qint64 savedChunkSize = obj.value(QStringLiteral("chunkSize")).toVariant().toLongLong();
        const int savedChunk = obj.value(QStringLiteral("nextChunk")).toInt();
        const QString savedFingerprint = obj.value(QStringLiteral("fingerprint")).toString();

        if (savedSize != expectedSize || savedChunkSize != chunkSize || savedChunk < 0)
            return false;
        if (savedFingerprint != transferFingerprint(safeName, expectedSize, chunkSize))
            return false;

        const QFileInfo partial(outputFileNameFor(safeName));
        const qint64 expectedPartialSize = qMin(expectedSize, (qint64)savedChunk * chunkSize);
        if (!partial.exists() || partial.size() < expectedPartialSize)
            return false;

        resumeChunk = savedChunk;
        return true;
    }

    QStringList parts = content.split(' ');
    if (parts.size() < 3) return false;

    qint64 savedSize = parts[0].toLongLong();
    qint64 savedChunkSize = parts[1].toLongLong();
    int savedChunk = parts[2].toInt();

    if (savedSize != expectedSize || savedChunkSize != chunkSize || savedChunk < 0)
        return false;

    const QFileInfo partial(outputFileNameFor(safeName));
    const qint64 expectedPartialSize = qMin(expectedSize, (qint64)savedChunk * chunkSize);
    if (!partial.exists() || partial.size() < expectedPartialSize)
        return false;

    resumeChunk = savedChunk;
    return true;
}

void NetworkManager::saveResumeInfo(const QString& fileName, qint64 fileSize,
                                    qint64 chunkSize, int nextChunk)
{
    QDir().mkpath(baseDataDir() + "/received_file/part");
    const QString safeName = safeTransferFileName(fileName);
    QFile f(resumeFileNameFor(safeName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject info;
        info["version"] = 2;
        info["fileName"] = safeName;
        info["fileSize"] = QString::number(fileSize);
        info["chunkSize"] = QString::number(chunkSize);
        info["nextChunk"] = nextChunk;
        info["fingerprint"] = transferFingerprint(safeName, fileSize, chunkSize);
        f.write(QJsonDocument(info).toJson(QJsonDocument::Compact));
        f.close();
    }
}

void NetworkManager::removeResumeInfo(const QString& fileName)
{
    QFile::remove(resumeFileNameFor(fileName));
}

bool NetworkManager::isSelfName(const QString& name) const
{
    QString clean = name.trimmed();
    clean.remove(QStringLiteral("(present)"));
    clean.remove(QStringLiteral("（当前设备）"));
    clean.remove(QStringLiteral("(当前设备)"));
    clean = clean.trimmed();
    return !m_myName.isEmpty() && clean == m_myName;
}

bool NetworkManager::waitIfTransferPaused(const QSharedPointer<TransferControl>& control) const
{
    if (!control)
        return true;
    QMutexLocker lock(&control->mtx);
    while (control->paused && !control->finished)
        control->cv.wait(&control->mtx, 250);
    return !control->finished;
}

void NetworkManager::markTransferFinished(int taskId)
{
    QSharedPointer<TransferControl> control = m_transferControls.value(taskId);
    if (control) {
        QMutexLocker lock(&control->mtx);
        control->finished = true;
        control->cv.wakeAll();
    }
}

bool NetworkManager::copyFileToReceivedDir(const QString& filePath, QString* savedFileName)
{
    const QString localFilePath = localPathFromUrl(filePath);
    QFileInfo src(localFilePath);
    if (!src.exists() || !src.isFile()) {
        emit notification(QStringLiteral("文件不存在: ") + localFilePath);
        return false;
    }

    QDir recvDir(receivedFilesDir());
    if (!recvDir.exists() && !recvDir.mkpath(QStringLiteral("."))) {
        emit notification(QStringLiteral("无法创建文件库目录: ") + recvDir.absolutePath());
        return false;
    }

    QString targetName = safeTransferFileName(src.fileName());
    QFileInfo targetInfo(targetName);
    QString baseName = targetInfo.completeBaseName();
    QString suffix = targetInfo.suffix();
    QString targetPath = recvDir.absoluteFilePath(targetName);
    int copyIndex = 1;
    while (QFile::exists(targetPath)) {
        targetName = suffix.isEmpty()
                         ? QStringLiteral("%1 (%2)").arg(baseName).arg(copyIndex)
                         : QStringLiteral("%1 (%2).%3").arg(baseName).arg(copyIndex).arg(suffix);
        targetPath = recvDir.absoluteFilePath(targetName);
        ++copyIndex;
    }

    if (!QFile::copy(src.absoluteFilePath(), targetPath)) {
        emit notification(QStringLiteral("保存文件失败: ") + targetPath);
        return false;
    }

    if (savedFileName)
        *savedFileName = targetName;
    return true;
}

QString NetworkManager::encodeProtocolToken(const QString& value) const
{
    if (!value.contains(QRegularExpression(QStringLiteral("[\\s%]"))))
        return value;
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

QString NetworkManager::decodeProtocolToken(const QString& value) const
{
    if (!value.contains('%'))
        return value;
    return QUrl::fromPercentEncoding(value.toLatin1());
}

bool NetworkManager::isUserOnline(const QString& name) const
{
    if (isSelfName(name))
        return true;

    const QString cleanName = name.trimmed();
    for (QString item : m_onlineUsersList) {
        item.remove(QStringLiteral("(present)"));
        item = item.trimmed();
        if (item == cleanName)
            return true;
    }

    QMutexLocker lock(const_cast<QMutex*>(&m_peersMutex));
    return m_discoveredPeers.contains(cleanName);
}

void NetworkManager::appendChatMessage(const QString& from, const QString& to, const QString& text,
                                       bool isMe, const QVariantMap& extra)
{
    QVariantMap msg = extra;
    msg["from"] = from;
    msg["to"] = to;
    msg["text"] = text;
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    msg["isMe"] = isMe;
    m_chatMessages.append(msg);
    saveChatHistory();
    emit chatMessagesChanged();
}

void NetworkManager::appendFileChatMessage(const QString& peer, const QString& fileName, qint64 fileSize,
                                           bool isMe, bool isLan)
{
    QVariantMap extra;
    extra["kind"] = QStringLiteral("file");
    extra["fileName"] = fileName;
    extra["fileSize"] = fileSize;
    if (isLan)
        extra["isLan"] = true;
    appendChatMessage(isMe ? m_myName : peer,
                      isMe ? peer : m_myName,
                      QStringLiteral("%1 (%2 MB)")
                          .arg(fileName)
                          .arg(fileSize / 1024.0 / 1024.0, 0, 'f', 2),
                      isMe,
                      extra);
}

void NetworkManager::removeLanChatMessages()
{
    QVariantList kept;
    bool changed = false;
    for (const QVariant& item : m_chatMessages) {
        const QVariantMap msg = item.toMap();
        if (isLanChatRecord(msg)) {
            changed = true;
            continue;
        }
        kept.append(item);
    }
    if (!changed)
        return;
    m_chatMessages = kept;
    saveChatHistory();
    emit chatMessagesChanged();
}

void NetworkManager::loadChatHistory()
{
    m_chatMessages.clear();
    if (m_connectionMode == LanOnlyMode)
        return;
    const QByteArray raw = m_chatSettings.value(chatHistoryKey()).toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray())
        return;

    const QJsonArray array = doc.array();
    bool removedLan = false;
    for (const QJsonValue& value : array) {
        const QVariantMap msg = value.toObject().toVariantMap();
        if (isLanChatRecord(msg)) {
            removedLan = true;
            continue;
        }
        m_chatMessages.append(msg);
    }
    if (removedLan)
        saveChatHistory();
}

void NetworkManager::saveChatHistory()
{
    if (m_connectionMode == LanOnlyMode)
        return;
    QJsonArray array;
    for (const QVariant& item : m_chatMessages) {
        const QVariantMap msg = item.toMap();
        if (isLanChatRecord(msg))
            continue;
        array.append(QJsonObject::fromVariantMap(msg));
    }
    m_chatSettings.setValue(chatHistoryKey(), QJsonDocument(array).toJson(QJsonDocument::Compact));
}

// ============================================================================
//  Transfer Display Helpers
// ============================================================================

void NetworkManager::updateTransferDisplay(int taskId, const TransferDisplayInfo& info)
{
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        m_transferDisplay[taskId] = info;
    }
    emit transfersChanged();
}

void NetworkManager::removeTransferDisplay(int taskId)
{
    {
        QMutexLocker lock(&m_transferDisplayMutex);
        m_transferDisplay.remove(taskId);
    }
    emit transfersChanged();
}

// Required for Q_OBJECT classes defined in .cpp
#include "NetworkManager.moc"
