#include "FileDownloader.hpp"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>

FileDownloader::FileDownloader(QObject *parent) : QObject(parent) {}

void FileDownloader::download(const QString &sharingUrl, const QString &destinationFilePath) {
    m_originalUrl = sharingUrl;
    m_destinationPath = destinationFilePath;

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    // 1. Handle Google Drive Sharing Links
    if (sharingUrl.contains("drive.google.com")) {
        parseGoogleDriveUrl(sharingUrl);
    } 
    // 2. Handle MediaFire Sharing Links (Requires scraping layout)
    else if (sharingUrl.contains("mediafire.com")) {
        QNetworkRequest request{QUrl(sharingUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0");
        
        m_currentReply = m_networkManager.get(request);
        connect(m_currentReply, &QNetworkReply::finished, this, &FileDownloader::onInitialPageFinished);
    } 
    // 3. Handle Standard Direct URLs (Validates if it points to an archive first)
    else {
        checkDirectUrl(QUrl(sharingUrl));
    }
}

void FileDownloader::parseGoogleDriveUrl(const QString &urlStr) {
    QRegularExpression rx("(?:/d/|[?&]id=)([a-zA-Z0-9_-]{25,})");
    QRegularExpressionMatch match = rx.match(urlStr);
    
    if (match.hasMatch()) {
        QString fileId = match.captured(1);
        QString directUrlStr = QString("https://drive.google.com/uc?export=download&confirm=t&id=%1").arg(fileId);
        processDirectUrl(QUrl(directUrlStr));
    } else {
        emit failed("Invalid Google Drive URL. Could not find a valid File ID.");
    }
}

void FileDownloader::onInitialPageFinished() {
    if (!m_currentReply) return;
    
    QByteArray data = m_currentReply->readAll();
    int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    if (m_currentReply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode < 600)) {
        QString err = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit failed(QString("Failed to contact server landing page: %1").arg(err));
        return;
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (m_originalUrl.contains("mediafire.com")) {
        parseMediaFirePage(QString::fromUtf8(data));
    }
}

void FileDownloader::parseMediaFirePage(const QString &htmlContent) {
    QRegularExpression rx("href=\"(https?://download[^\"]+)\"[^>]*id=\"downloadButton\"");
    QRegularExpressionMatch match = rx.match(htmlContent);
    
    if (!match.hasMatch()) {
        rx.setPattern("href=\"(https?://download\\d+\\.mediafire\\.com/[^\"]+)\"");
        match = rx.match(htmlContent);
    }

    if (match.hasMatch()) {
        processDirectUrl(QUrl(match.captured(1)));
    } else {
        emit failed("Could not parse MediaFire direct download link. Structure may have changed or file is invalid.");
    }
}

void FileDownloader::checkDirectUrl(const QUrl &directUrl) {
    QNetworkRequest request(directUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

    // Perform a lightweight HEAD request to inspect metadata before pulling payload bytes
    m_currentReply = m_networkManager.head(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &FileDownloader::onHeadFinished);
}

void FileDownloader::onHeadFinished() {
    if (!m_currentReply) return;

    int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QUrl finalUrl = m_currentReply->url(); // Captures final target URL if redirected

    if (m_currentReply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode < 600)) {
        QString err = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit failed(QString("Direct URL validation failed: %1").arg(err));
        return;
    }

    bool isValidArchive = isArchiveFile(finalUrl, m_currentReply);

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (isValidArchive) {
        processDirectUrl(finalUrl);
    } else {
        emit failed("Validation failed: The direct URL does not point to a supported archive file format (e.g., .zip, .rar, .7z, .tar, .gz).");
    }
}

bool FileDownloader::isArchiveFile(const QUrl &url, QNetworkReply *headReply) {
    QString path = url.path().toLower();
    static const QStringList archiveExtensions = {".zip", ".rar", ".7z", ".tar", ".gz", ".bz2", ".xz", ".tgz", ".tbz2", ".zst", ".iso"};

    // 1. Check extension from URL path
    for (const QString &ext : archiveExtensions) {
        if (path.endsWith(ext)) {
            return true;
        }
    }

    if (headReply) {
        // 2. Check Content-Disposition header (often contains filename="...")
        QString contentDisp = headReply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
        for (const QString &ext : archiveExtensions) {
            if (contentDisp.contains(ext)) {
                return true;
            }
        }

        // 3. Check Content-Type header keywords
        QString contentType = headReply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        if (contentType.contains("zip") 
            //|| contentType.contains("tar") 
            || contentType.contains("compressed") 
            //|| contentType.contains("rar")
            //|| contentType.contains("7z")
        ) {
            return true;
        }
    }

    return false;
}

void FileDownloader::processDirectUrl(const QUrl &directUrl) {
    QNetworkRequest request(directUrl);
    // FollowRedirectAttribute is omitted; Qt6 handles safe redirects automatically.
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

