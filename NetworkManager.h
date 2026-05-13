#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSharedPointer>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QAtomicInt>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QSet>

// ============ Protocol Constants (must match server.cpp / data_server.cpp) ============

#define ECS_SERVER_IP   "47.120.32.86"
#define ECS_CTRL_PORT   7000
#define ECS_DATA_PORT   7001
#define LAN_CHUNK_SIZE  (8 * 1024 * 1024)    // 8 MB
#define ECS_CHUNK_SIZE  (512 * 1024)         // 0.5 MB
#define MAX_CHUNK_SIZE  LAN_CHUNK_SIZE
#define CTRL_BUFFER_SIZE 4096

#define LAN_DATA_PORT       9000
#define LAN_DISCOVERY_PORT  9001
#define LAN_CTRL_PORT       9002

// ============ Binary Protocol Structures ============

#pragma pack(push, 1)
struct ChunkHeader {
    qint32 task_id;
    qint32 chunk_index;
    qint32 data_size;
};
#pragma pack(pop)

// ============ Internal Data Structures ============

struct SendTaskInfo {
    int taskId = 0;
    QString peerName;
    QString fileName;
    qint64 fileSize = 0;
    int totalChunks = 0;
    int resumeChunk = 0;
    bool isLan = false;
    QString lanPeerIp;
    int lanDataPort = LAN_DATA_PORT;

    // Cross-thread state (shared between main thread and send worker)
    int lastAckedChunk = -1;
    bool paused = false;
    bool finished = false;
    QMutex mtx;
    QWaitCondition cv;
};

struct TransferControl {
    bool paused = false;
    bool finished = false;
    QMutex mtx;
    QWaitCondition cv;
};

struct LanAcceptedTask {
    QString fileName;
    qint64 fileSize;
    int resumeChunk;
};

struct IncomingOffer {
    bool valid = false;
    bool isLan = false;
    QTcpSocket* lanSocket = nullptr;
    int taskId = 0;
    QString fromUser;
    QString fileName;
    qint64 fileSize = 0;
};

struct OutgoingFileReq {
    QString toUser;
    QString fileName;
    qint64 fileSize;
};

struct TransferDisplayInfo {
    int taskId = 0;
    QString fileName;
    qint64 fileSize = 0;
    qint64 bytesTransferred = 0;
    double speedMBps = 0;
    QString state;      // "sending", "receiving", "paused", "completed", "failed"
    QString peerName;
    bool isLan = false;
    QString direction;  // "send" or "recv"
};

// ============ NetworkManager ============

class NetworkManager : public QObject {
    Q_OBJECT

    // ---- Connection State ----
    Q_PROPERTY(int connectionMode READ connectionMode NOTIFY connectionModeChanged)
    Q_PROPERTY(QString myName READ myName NOTIFY myNameChanged)
    Q_PROPERTY(bool ecsConnected READ ecsConnected NOTIFY ecsConnectedChanged)

    // ---- Peer / User Lists ----
    Q_PROPERTY(QVariantList discoveredPeers READ discoveredPeers NOTIFY peersChanged)
    Q_PROPERTY(QVariantList onlineUsers READ onlineUsers NOTIFY onlineUsersChanged)

    // ---- Transfer Tasks ----
    Q_PROPERTY(QVariantList transfers READ transfers NOTIFY transfersChanged)

    // ---- Incoming File Offer ----
    Q_PROPERTY(bool hasIncomingOffer READ hasIncomingOffer NOTIFY incomingOfferChanged)
    Q_PROPERTY(QString offerFrom READ offerFrom NOTIFY incomingOfferChanged)
    Q_PROPERTY(QString offerFileName READ offerFileName NOTIFY incomingOfferChanged)
    Q_PROPERTY(qint64 offerFileSize READ offerFileSize NOTIFY incomingOfferChanged)
    Q_PROPERTY(bool offerIsLan READ offerIsLan NOTIFY incomingOfferChanged)

    // ---- Chat Messages ----
    Q_PROPERTY(QVariantList chatMessages READ chatMessages NOTIFY chatMessagesChanged)
    Q_PROPERTY(QString receivedFilesDir READ receivedFilesDir NOTIFY receivedFilesDirChanged)

public:
    enum ConnectionMode {
        Disconnected = 0,
        EcsMode = 1,
        LanOnlyMode = 2
    };
    Q_ENUM(ConnectionMode)

    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    // ---- Property Getters ----
    int connectionMode() const;
    QString myName() const;
    bool ecsConnected() const;
    QVariantList discoveredPeers() const;
    QVariantList onlineUsers() const;
    QVariantList transfers() const;
    bool hasIncomingOffer() const;
    QString offerFrom() const;
    QString offerFileName() const;
    qint64 offerFileSize() const;
    bool offerIsLan() const;
    QVariantList chatMessages() const;

    // ---- QML Invokable Methods ----

    // Login / Mode
    Q_INVOKABLE void loginEcs(const QString& user, const QString& pass);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void startLanMode(const QString& displayName);

    // LAN Discovery
    Q_INVOKABLE void discover(bool showNotification = true);

    // File Transfer
    Q_INVOKABLE void lanSendFile(const QString& peerNameOrIp, const QString& filePath);
    Q_INVOKABLE void ecsSendFile(const QString& toUser, const QString& filePath);

    // ECS Messaging
    Q_INVOKABLE void ecsSendMessage(const QString& toUser, const QString& message);
    Q_INVOKABLE void ecsListUsers();
    Q_INVOKABLE QString localPathFromUrl(const QString& urlOrPath) const;
    Q_INVOKABLE QString desktopDir() const;
    Q_INVOKABLE void ensureConversation(const QString& peerName);

    // Incoming Offer
    Q_INVOKABLE void acceptOffer();
    Q_INVOKABLE void rejectOffer();

    // Pause / Resume
    Q_INVOKABLE void pauseTask(int taskId);
    Q_INVOKABLE void resumeTask(int taskId);
    Q_INVOKABLE void pauseAll();
    Q_INVOKABLE void resumeAll();

    // Utility
    Q_INVOKABLE QString receivedFilesDir() const;
    Q_INVOKABLE void clearLocalData();
    Q_INVOKABLE void clearAllLocalData();

signals:
    // Property change signals
    void connectionModeChanged();
    void myNameChanged();
    void ecsConnectedChanged();
    void peersChanged();
    void onlineUsersChanged();
    void transfersChanged();
    void incomingOfferChanged();
    void chatMessagesChanged();
    void receivedFilesDirChanged();

    // Action result signals
    void loginResult(bool success, const QString& error);
    void notification(const QString& message);
    void transferCompleted(int taskId, const QString& fileName);
    void transferFailed(int taskId, const QString& error);

private slots:
    // ECS control socket
    void onEcsConnected();
    void onEcsReadyRead();
    void onEcsDisconnected();
    void onEcsError(QAbstractSocket::SocketError error);

    // UDP discovery
    void onUdpReadyRead();

    // LAN servers
    void onLanCtrlNewConnection();
    void onLanDataNewConnection();

    // Worker thread callbacks (connected via Qt::QueuedConnection)
    void onWorkerProgress(int taskId, qint64 bytesTransferred, double speedMBps);
    void onWorkerFinished(int taskId);
    void onWorkerError(int taskId, const QString& message);
    void onWorkerSendCtrl(const QString& message);

private:
    // ---- Initialization ----
    void initLanServers();

    // ---- ECS Message Handling ----
    void handleServerLine(const QString& line);
    void sendCtrlText(const QString& text);

    // ---- File / Resume Helpers ----
    QString appRootDir() const;
    QString accountFolderName() const;
    QString chatHistoryKey() const;
    QString baseDataDir() const;
    QString outputFileNameFor(const QString& fileName) const;
    QString resumeFileNameFor(const QString& fileName) const;
    QString safeTransferFileName(const QString& fileName) const;
    QString transferFingerprint(const QString& fileName, qint64 fileSize, qint64 chunkSize) const;
    bool loadResumeInfo(const QString& fileName, qint64 expectedSize,
                        qint64 chunkSize, int& resumeChunk);
    void saveResumeInfo(const QString& fileName, qint64 fileSize,
                        qint64 chunkSize, int nextChunk);
    void removeResumeInfo(const QString& fileName);
    bool isSelfName(const QString& name) const;
    bool waitIfTransferPaused(const QSharedPointer<TransferControl>& control) const;
    void markTransferFinished(int taskId);
    bool copyFileToReceivedDir(const QString& filePath, QString* savedFileName = nullptr);
    QString encodeProtocolToken(const QString& value) const;
    QString decodeProtocolToken(const QString& value) const;
    bool isUserOnline(const QString& name) const;
    void appendChatMessage(const QString& from, const QString& to, const QString& text,
                           bool isMe, const QVariantMap& extra = QVariantMap());
    void appendFileChatMessage(const QString& peer, const QString& fileName, qint64 fileSize,
                               bool isMe, bool isLan = false);
    void removeLanChatMessages();
    void loadChatHistory();
    void saveChatHistory();

    // ---- Transfer Display ----
    void updateTransferDisplay(int taskId, const TransferDisplayInfo& info);
    void removeTransferDisplay(int taskId);

    // ---- Start Workers ----
    void startSendWorker(QSharedPointer<SendTaskInfo> task);
    void startRecvWorker(int taskId, const QString& fileName, qint64 fileSize,
                         int resumeChunk, QTcpSocket* lanDataSocket, bool isLan);
    void stopAllWorkers();

    // ========== Member Variables ==========

    // Connection
    int m_connectionMode = Disconnected;
    QString m_myName;
    bool m_ecsConnected = false;
    QString m_pendingLoginUser;
    QString m_pendingLoginPass;

    // ECS control socket (main thread only)
    QTcpSocket* m_ecsSocket = nullptr;
    QString m_ecsPending;   // partial-line buffer

    // LAN
    QUdpSocket* m_udpSocket = nullptr;
    QTcpServer* m_lanCtrlServer = nullptr;
    QTcpServer* m_lanDataServer = nullptr;
    int m_myLanCtrlPort = LAN_CTRL_PORT;
    int m_myLanDataPort = LAN_DATA_PORT;

    // Discovered peers:  name -> "ip:port"
    QMap<QString, QString> m_discoveredPeers;
    QMutex m_peersMutex;

    // ECS online users
    QStringList m_onlineUsersList;

    // Send tasks (shared with worker threads)
    QMap<int, QSharedPointer<SendTaskInfo>> m_sendTasks;
    QMutex m_sendTasksMutex;

    // Pending ECS file requests
    QMap<QString, OutgoingFileReq> m_pendingRequests;

    // LAN accepted tasks (awaiting data connection)
    QMap<int, LanAcceptedTask> m_lanAcceptedTasks;
    QMutex m_lanAcceptedMutex;

    // Incoming offer
    IncomingOffer m_incomingOffer;

    // Active worker threads
    QMap<int, QThread*> m_workerThreads;
    QMap<int, QSharedPointer<TransferControl>> m_transferControls;

    // Transfer display for QML
    QMap<int, TransferDisplayInfo> m_transferDisplay;
    QMutex m_transferDisplayMutex;

    // Chat messages
    QVariantList m_chatMessages;
    QSettings m_chatSettings;

    // Task ID counter for LAN
    QAtomicInt m_lanLocalTaskId{5000};

    bool m_running = true;
};

#endif // NETWORKMANAGER_H
