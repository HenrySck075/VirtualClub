#include "FileDownloader.hpp"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/dom/collection.h>
#include <QUrlQuery>

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
        // Request the base uc endpoint. Small files download directly; 
        // large files trigger the virus warning HTML page.
        QString initialUrlStr = QString("https://drive.google.com/uc?export=download&id=%1").arg(fileId);
        
        QNetworkRequest request{QUrl(initialUrlStr)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        
        m_currentReply = m_networkManager.get(request);
        connect(m_currentReply, &QNetworkReply::finished, this, &FileDownloader::onInitialPageFinished);
    } else {
        emit failed("Invalid Google Drive URL. Could not find a valid File ID.");
    }
}

void FileDownloader::onInitialPageFinished() {
    if (!m_currentReply) return;
    
    QByteArray data = m_currentReply->readAll();
    int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString contentType = m_currentReply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    
    if (m_currentReply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode < 600)) {
        QString err = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit failed(QString("Failed to contact server landing page: %1").arg(err));
        return;
    }
    
    // If it's Google Drive and Content-Type is NOT HTML, it's a small file 
    // that downloaded directly on the initial GET request.
    if (m_originalUrl.contains("drive.google.com") && !contentType.contains("text/html")) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;

        m_saveFile = std::make_unique<QSaveFile>(m_destinationPath);
        if (!m_saveFile->open(QIODevice::WriteOnly)) {
            emit failed(QString("Could not open destination file path for writing: %1").arg(m_destinationPath));
            return;
        }
        m_saveFile->write(data);
        if (m_saveFile->commit()) {
            emit finished(m_destinationPath);
        } else {
            emit failed("Failed to commit downloaded bytes to storage.");
        }
        return;
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (m_originalUrl.contains("mediafire.com")) {
        parseMediaFirePage(QString::fromUtf8(data));
    } else if (m_originalUrl.contains("drive.google.com")) {
        parseGoogleDrivePage(QString::fromUtf8(data));
    }
}

void FileDownloader::parseGoogleDrivePage(const QString &htmlContent) {
    QByteArray utf8Data = htmlContent.toUtf8();

    // 1. Initialize Lexbor HTML5 document
    lxb_html_document_t *document = lxb_html_document_create();
    if (!document) {
        emit failed("Failed to create Lexbor HTML document instance.");
        return;
    }

    lxb_status_t status = lxb_html_document_parse(document, (const lxb_char_t *)utf8Data.constData(), utf8Data.size());
    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(document);
        emit failed("Lexbor failed to parse Google Drive warning page HTML.");
        return;
    }

    // 2. Create a DOM element collection to search elements
    lxb_dom_collection_t *collection = lxb_dom_collection_make(&document->dom_document, 16);
    if (!collection) {
        lxb_html_document_destroy(document);
        emit failed("Failed to allocate Lexbor DOM collection.");
        return;
    }

    lxb_dom_element_t *root_element = lxb_dom_interface_element(document->body);

    // 3. Find the download form action URL
    status = lxb_dom_elements_by_tag_name(root_element, collection, (const lxb_char_t *)"form", 4);
    QString actionUrl;

    if (status == LXB_STATUS_OK && lxb_dom_collection_length(collection) > 0) {
        lxb_dom_element_t *form_elem = lxb_dom_collection_element(collection, 0);
        size_t val_len = 0;
        const lxb_char_t *action_val = lxb_dom_element_get_attribute(form_elem, (const lxb_char_t *)"action", 6, &val_len);
        if (action_val) {
            actionUrl = QString::fromUtf8((const char *)action_val, val_len);
        }
    }

    lxb_dom_collection_clean(collection);

    if (actionUrl.isEmpty()) {
        lxb_dom_collection_destroy(collection, true);
        lxb_html_document_destroy(document);
        emit failed("Could not locate download form action URL via Lexbor parser.");
        return;
    }

    actionUrl.replace("&amp;", "&");
    if (actionUrl.startsWith("/")) {
        actionUrl = "https://drive.google.com" + actionUrl;
    }

    QUrl url(actionUrl);
    QUrlQuery query;

    // 4. Find all <input> elements to sweep up hidden tokens (uuid, at, confirm, id)
    status = lxb_dom_elements_by_tag_name(root_element, collection, (const lxb_char_t *)"input", 5);
    if (status == LXB_STATUS_OK) {
        size_t length = lxb_dom_collection_length(collection);
        for (size_t i = 0; i < length; ++i) {
            lxb_dom_element_t *input_elem = lxb_dom_collection_element(collection, i);
            size_t name_len = 0, val_len = 0, type_len = 0;

            const lxb_char_t *type_val = lxb_dom_element_get_attribute(input_elem, (const lxb_char_t *)"type", 4, &type_len);
            QString type = type_val ? QString::fromUtf8((const char *)type_val, type_len).toLower() : QString();

            // Target hidden tracking inputs and submission triggers
            if (type == "hidden" || type == "submit" || type.isEmpty()) {
                const lxb_char_t *name_val = lxb_dom_element_get_attribute(input_elem, (const lxb_char_t *)"name", 4, &name_len);
                if (name_val) {
                    QString name = QString::fromUtf8((const char *)name_val, name_len);
                    const lxb_char_t *value_val = lxb_dom_element_get_attribute(input_elem, (const lxb_char_t *)"value", 5, &val_len);
                    QString value = value_val ? QString::fromUtf8((const char *)value_val, val_len) : QString();

                    query.addQueryItem(name, value);
                }
            }
        }
    }

    // Clean up Lexbor allocations cleanly
    lxb_dom_collection_destroy(collection, true);
    lxb_html_document_destroy(document);

    url.setQuery(query);

    // 5. Trigger download with fully compiled endpoint and parameters
    processDirectUrl(url);
}

void FileDownloader::parseMediaFirePage(const QString &htmlContent) {
    QByteArray utf8Data = htmlContent.toUtf8();

    // 1. Initialize Lexbor HTML5 document
    lxb_html_document_t *document = lxb_html_document_create();
    if (!document) {
        emit failed("Failed to create Lexbor HTML document instance for MediaFire.");
        return;
    }

    lxb_status_t status = lxb_html_document_parse(document, (const lxb_char_t *)utf8Data.constData(), utf8Data.size());
    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(document);
        emit failed("Lexbor failed to parse MediaFire landing page HTML.");
        return;
    }

    // 2. Create a DOM collection to search elements
    lxb_dom_collection_t *collection = lxb_dom_collection_make(&document->dom_document, 16);
    if (!collection) {
        lxb_html_document_destroy(document);
        emit failed("Failed to allocate Lexbor DOM collection for MediaFire.");
        return;
    }

    lxb_dom_element_t *root_element = lxb_dom_interface_element(document->body);

    // 3. Find the download button using tag and ID lookup
    // Target structure: form[name="download"] > div#download_link > a#downloadButton
    status = lxb_dom_elements_by_tag_name(root_element, collection, (const lxb_char_t *)"a", 1);
    QString downloadUrl;

    if (status == LXB_STATUS_OK) {
        size_t length = lxb_dom_collection_length(collection);
        for (size_t i = 0; i < length; ++i) {
            lxb_dom_element_t *elem = lxb_dom_collection_element(collection, i);
            size_t id_len = 0;
            
            // Check if element has id="downloadButton"
            const lxb_char_t *id_val = lxb_dom_element_get_attribute(elem, (const lxb_char_t *)"id", 2, &id_len);
            if (id_val && QString::fromUtf8((const char *)id_val, id_len) == "downloadButton") {
                size_t href_len = 0;
                const lxb_char_t *href_val = lxb_dom_element_get_attribute(elem, (const lxb_char_t *)"href", 4, &href_len);
                if (href_val) {
                    downloadUrl = QString::fromUtf8((const char *)href_val, href_len);
                    break;
                }
            }
        }
    }

    // Clean up Lexbor allocations
    lxb_dom_collection_destroy(collection, true);
    lxb_html_document_destroy(document);

    if (downloadUrl.isEmpty()) {
        emit failed("Could not locate MediaFire direct download link via Lexbor parser. Structure may have changed.");
        return;
    }

    downloadUrl.replace("&amp;", "&");
    processDirectUrl(QUrl(downloadUrl));
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
    qDebug() << "Downloading" << directUrl;
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

