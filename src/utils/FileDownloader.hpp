#pragma once

#include <QObject>

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
  
  // Starts the download process for Google Drive, MediaFire, or direct URLs
  void download(const QString &sharingUrl, const QString &destinationFilePath);

signals:
  void progress(qint64 bytesReceived, qint64 bytesTotal);
  void finished(const QString &filePath);
  void failed(const QString &errorString);

private slots:
  void onInitialPageFinished();
  void onHeadFinished();
  void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void onDownloadFinished();

private:
  void checkDirectUrl(const QUrl &directUrl);
  void processDirectUrl(const QUrl &directUrl);
  void parseGoogleDrivePage(const QString &htmlContent);
  void parseMediaFirePage(const QString &htmlContent);
  void parseGoogleDriveUrl(const QString &urlStr);
  bool isArchiveFile(const QUrl &url, QNetworkReply *headReply);

  QNetworkAccessManager m_networkManager;
  QNetworkReply *m_currentReply = nullptr;
  std::unique_ptr<QSaveFile> m_saveFile;
  QString m_destinationPath;
  QString m_originalUrl;
};
