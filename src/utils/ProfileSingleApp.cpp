#include "ProfileSingleApp.hpp"
#include <QDataStream>
#include <QCryptographicHash>

ProfileSingleApp::ProfileSingleApp(int &argc, char **argv)
    : QApplication(argc, argv)
{
}

ProfileSingleApp::~ProfileSingleApp() {
    if (m_localServer) {
        m_localServer->close();
    }
    if (m_lockFile && m_lockFile->isLocked()) {
        m_lockFile->unlock();
        delete m_lockFile;
    }
}

bool ProfileSingleApp::notifyPrimaryInstance(const QStringList &args) {
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    
    if (!socket.waitForConnected(1000)) {
        return false;
    }

    // Send arguments to the running primary instance for this profile
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << args;

    socket.write(block);
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    
    return true;
}
const void ProfileSingleApp::setProfileId(const QString &profileId) {
  if (m_initComplete) return;

  m_profileId = profileId;
    // 1. Hash profile ID to create a safe cross-platform resource string
    QByteArray hash = QCryptographicHash::hash(m_profileId.toUtf8(), QCryptographicHash::Sha256).toHex();
    m_serverName = QString("vcprofile_%1").arg(QString(hash));

    // 2. Lock file path scoped per profile
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                       + QString("/vcprofile_%1.lock").arg(QString(hash));
    
    m_lockFile = new QLockFile(lockPath);
    m_lockFile->setStaleLockTime(0); // Prevent delayed takeover if crashed

    // 3. Try to acquire the primary lock for this profile
    if (!m_lockFile->tryLock()) {
        // Another process is already running this profile
        m_isSecondary = true;
        return;
    }

    // 4. If we locked it successfully, set up the IPC Server to listen for future secondary instances
    QLocalServer::removeServer(m_serverName); // Clean up stale sockets
    m_localServer = new QLocalServer(this);
    
    if (m_localServer->listen(m_serverName)) {
        connect(m_localServer, &QLocalServer::newConnection, this, &ProfileSingleApp::handleNewConnection);
    }

  m_initComplete = true;

}

void ProfileSingleApp::handleNewConnection() {
    QLocalSocket *socket = m_localServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        QDataStream in(socket);
        in.setVersion(QDataStream::Qt_6_0);

        QStringList receivedArgs;
        in >> receivedArgs;

        emit messageReceivedFromSecondary(receivedArgs);
        socket->deleteLater();
    });
}
