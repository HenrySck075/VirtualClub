#ifndef PROFILESINGLEAPP_H
#define PROFILESINGLEAPP_H

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QDir>
#include <QStandardPaths>
#include <qsharedpointer.h>

class ProfileSingleApp : public QApplication {
    Q_OBJECT
public:
    ProfileSingleApp(int &argc, char **argv);
    ~ProfileSingleApp();

    bool isSecondary() const { return m_isSecondary; }
    bool notifyPrimaryInstance(const QStringList &args);

    const QString &profileId() const { return m_profileId; }
    // TODO: single-call function
    const void setProfileId(const QString &profileId);
    // why not
    static inline ProfileSingleApp* instance() {
      return qobject_cast<ProfileSingleApp*>(QApplication::instance());
    }

signals:
    void messageReceivedFromSecondary(const QStringList &args);

private slots:
    void handleNewConnection();

private:
    QString m_profileId;
    QString m_serverName;
    bool m_isSecondary = false;

    bool m_initComplete = false;

    QLockFile *m_lockFile = nullptr;
    QLocalServer *m_localServer = nullptr;
};

#endif // PROFILESINGLEAPP_H
