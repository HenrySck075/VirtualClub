#include "ProfileSingleApp.hpp"
#include "consts.hpp"
#include "utils/ProfileSettings.hpp"
#include <QDataStream>
#include <QCryptographicHash>
#include <QDebug>

ProfileSingleApp::ProfileSingleApp(int &argc, char **argv)
    : PSA_BaseQApplication(argc, argv)
{
}

ProfileSingleApp::~ProfileSingleApp() {
    if (m_localServer) {
        m_localServer->close();
    }
    if (m_lockFile) {
        if (m_lockFile->isLocked()) {
            m_lockFile->unlock();
        }
        delete m_lockFile;
        m_lockFile = nullptr;
    }
}

bool ProfileSingleApp::notifyPrimaryInstance(const QStringList &args) {
    qDebug() << "ProfileSingleApp: notifying primary instance" << m_serverName
             << "with args" << args;
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    
    if (!socket.waitForConnected(1000)) {
        qWarning() << "ProfileSingleApp: could not connect to primary instance"
                   << m_serverName;
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
    qDebug() << "ProfileSingleApp: notification sent";
    
    return true;
}


QString ProfileSingleApp::profileName() {
  return ProfileSettings::getOf(m_profileId)->value(STK_DISPLAYNAME, m_profileId).toString();
}

const void ProfileSingleApp::setProfileId(const QString &profileId) {
  if (m_initComplete) {
    qWarning() << "ProfileSingleApp: profile ID change ignored after initialization";
    return;
  }
  qDebug() << "ProfileSingleApp: initializing profile" << profileId;

  m_profileId = profileId;
    // 1. Hash profile ID to create a safe cross-platform resource string
    QByteArray hash = QCryptographicHash::hash(m_profileId.toUtf8(), QCryptographicHash::Sha256).toHex();
    m_serverName = QString("vcprofile_%1").arg(QString(hash));

    // 2. Lock file path scoped per profile
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                       + QString("/vcprofile_%1.lock").arg(QString(hash));
    
    m_lockFile = new QLockFile(lockPath);

    // 3. Try to acquire the primary lock for this profile
    if (!m_lockFile->tryLock()) {
        qWarning() << "ProfileSingleApp: profile lock already held:" << profileId;
        // Another process is already running this profile
        m_isSecondary = true;
        return;
    }

    // 4. If we locked it successfully, set up the IPC Server to listen for future secondary instances
    QLocalServer::removeServer(m_serverName); // Clean up stale sockets
    m_localServer = new QLocalServer(this);
    
    if (m_localServer->listen(m_serverName)) {
        qDebug() << "ProfileSingleApp: IPC server listening on" << m_serverName;
        connect(m_localServer, &QLocalServer::newConnection, this, &ProfileSingleApp::handleNewConnection);
    } else {
        qWarning() << "ProfileSingleApp: could not start IPC server" << m_serverName;
    }

  m_initComplete = true;

}

void ProfileSingleApp::handleNewConnection() {
    QLocalSocket *socket = m_localServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        QByteArray &pending = m_pendingMessages[socket];
        pending.append(socket->readAll());

        QDataStream in(pending);
        in.setVersion(QDataStream::Qt_6_0);

        QStringList receivedArgs;
        in >> receivedArgs;
        if (in.status() != QDataStream::Ok) {
            return;
        }

        m_pendingMessages.remove(socket);
        emit messageReceivedFromSecondary(receivedArgs);
        socket->deleteLater();
    });

    connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
        m_pendingMessages.remove(socket);
        socket->deleteLater();
    });
}
