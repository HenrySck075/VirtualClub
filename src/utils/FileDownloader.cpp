#include "FileDownloader.hpp"
#include <QDebug>

FileDownloader::FileDownloader(QObject *parent) : QObject(parent) {}

void FileDownloader::cancel() {
    if (m_cancelled) return;
    m_cancelled = true;

    if (m_saveFile) {
        m_saveFile->cancelWriting();
        m_saveFile.reset();
    }

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    emit cancelled();
}

void FileDownloader::download(const QUrl &url, const QString &destinationFilePath) {
    m_cancelled = false;
    qDebug() << "FileDownloader: download requested from" << url
             << "to" << destinationFilePath;
    m_destinationPath = destinationFilePath;

    if (m_currentReply) {
        
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_cancelled) return;
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

    m_saveFile = std::make_unique<QSaveFile>(m_destinationPath);
    if (!m_saveFile->open(QIODevice::WriteOnly)) {
        emit failed(QString("Could not open destination file path for writing: %1").arg(m_destinationPath));
        return;
    }

    m_currentReply = m_networkManager.get(request);
    
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &FileDownloader::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &FileDownloader::onDownloadFinished);
}

void FileDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    emit progress(bytesReceived, bytesTotal);
}

void FileDownloader::onDownloadFinished() {
    if (!m_currentReply) return;
    if (m_cancelled) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        m_saveFile.reset();
        return;
    }

    if (m_currentReply->error() == QNetworkReply::NoError) {
        m_saveFile->write(m_currentReply->readAll());
        if (m_saveFile->commit()) {
            emit finished(m_destinationPath);
        } else {
            emit failed("Failed to commit downloaded bytes to storage.");
        }
    } else {
        QString err = m_currentReply->errorString();
        m_saveFile->cancelWriting();
        emit failed(QString("Download stream interrupted: %1").arg(err));
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

