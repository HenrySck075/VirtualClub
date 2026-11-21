#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <memory>

class FileDownloader : public QObject {
  Q_OBJECT
public:
  explicit FileDownloader(QObject *parent = nullptr);
  
  void download(const QUrl &url, const QString &destinationFilePath);
  void cancel();

signals:
  void progress(qint64 bytesReceived, qint64 bytesTotal);
  void finished(const QString &filePath);
  void failed(const QString &errorString);
  void cancelled();

private slots:
  void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void onDownloadFinished();

private:
  QNetworkAccessManager m_networkManager;
  QNetworkReply *m_currentReply = nullptr;
  std::unique_ptr<QSaveFile> m_saveFile;
  QString m_destinationPath;
  bool m_cancelled = false;
};
