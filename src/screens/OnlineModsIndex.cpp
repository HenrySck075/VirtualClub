#include "OnlineModsIndex.hpp"
#include "MainWindow.hpp"
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainterPath>
#include <QLabel>
#include <QLineEdit>
#include "ui/ElidedLabel.hpp"
#include "ui/GradientBackground.hpp"
#include "ui/IconButton.hpp"
#include "ui/ImageLoadingWidget.hpp"
#include "ui/MESWidgets.hpp"
#include "ui/PixmapWidget.hpp"
#include "utils/FlowLayout.hpp"
#include "utils/LucideIcons.hpp"
#include "ui/Router.hpp"
#include "ui/LoadingCircle.hpp"
#include "utils/ModIndex.hpp"
#include "utils/OnlineModsIndex.hpp"
#include "utils/utils.hpp"
#include <QComboBox>
#include <QMouseEvent>
#include <QPointer>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProgressBar>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <QUrlQuery>
#include <string_view>
#include <archive.h>
#include <archive_entry.h>
#include "utils/FileDownloader.hpp"
extern "C" {
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/dom/collection.h>
}
#include "consts.hpp"
#include "utils/i18n.hpp"

// fuck you windows in particular
#undef min
#undef max

namespace {
bool isSafeArchivePath(const char* filename) {
  const std::filesystem::path path(filename);
  if (path.is_absolute()) {
    return false;
  }
  for (const auto& component : path) {
    if (component == "..") {
      return false;
    }
  }
  return true;
}

int countArchiveEntries(const char* archiveName) {
  auto* archive = archive_read_new();
  archive_read_support_filter_all(archive);
  archive_read_support_format_all(archive);
  if (archive_read_open_filename(archive, archiveName, 10240) != ARCHIVE_OK) {
    archive_read_free(archive);
    return -1;
  }

  int count = 0;
  archive_entry* entry = nullptr;
  while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
    ++count;
    archive_read_data_skip(archive);
  }
  const int result = archive_read_free(archive);
  return result == ARCHIVE_OK ? count : -1;
}

template <typename F>
int extractArchive(const char* archiveName, const char* extractDirectory,
                   F&& callback, std::string& error) {
  auto* archive = archive_read_new();
  archive_read_support_filter_all(archive);
  archive_read_support_format_all(archive);
  if (archive_read_open_filename(archive, archiveName, 10240) != ARCHIVE_OK) {
    error = archive_error_string(archive);
    archive_read_free(archive);
    return -1;
  }

  auto* disk = archive_write_disk_new();
  archive_write_disk_set_options(
      disk, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS |
                ARCHIVE_EXTRACT_SECURE_SYMLINKS);
  archive_write_disk_set_standard_lookup(disk);

  const std::filesystem::path destination(extractDirectory);
  archive_entry* entry = nullptr;
  int result = ARCHIVE_OK;
  while ((result = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
    const char* filename = archive_entry_pathname(entry);
    if (filename == nullptr || !isSafeArchivePath(filename)) {
      error = "Archive contains an unsafe path.";
      result = ARCHIVE_FATAL;
      break;
    }
    const std::string memberName(filename);

    const auto outputPath = destination / std::filesystem::path(memberName);
    archive_entry_set_pathname(entry, outputPath.string().c_str());
    result = archive_write_header(disk, entry);
    if (result < ARCHIVE_WARN) {
      error = archive_error_string(disk);
      break;
    }

    const void* buffer = nullptr;
    size_t size = 0;
    la_int64_t offset = 0;
    int readResult = ARCHIVE_OK;
    while ((readResult = archive_read_data_block(archive, &buffer, &size,
                                                 &offset)) == ARCHIVE_OK) {
      if (archive_write_data(disk, buffer, size) < 0) {
        error = archive_error_string(disk);
        result = ARCHIVE_FATAL;
        break;
      }
    }
    if (result == ARCHIVE_FATAL) {
      break;
    }
    if (readResult != ARCHIVE_EOF) {
      error = archive_error_string(archive);
      result = ARCHIVE_FATAL;
      break;
    }
    result = archive_write_finish_entry(disk);
    if (result < ARCHIVE_WARN) {
      error = archive_error_string(disk);
      break;
    }
    callback(memberName.c_str());
  }

  if (result != ARCHIVE_EOF && result >= ARCHIVE_WARN && error.empty()) {
    error = archive_error_string(archive);
  }
  archive_write_free(disk);
  archive_read_free(archive);
  return error.empty() ? 0 : -1;
}
}

class OnlineModDownloader : public QObject {
  Q_OBJECT
public:
  explicit OnlineModDownloader(QObject* parent = nullptr)
    : QObject(parent), m_downloader(new FileDownloader(this)) {
    connect(m_downloader, &FileDownloader::progress, this, &OnlineModDownloader::progress);
    connect(m_downloader, &FileDownloader::failed, this, &OnlineModDownloader::failed);
    connect(m_downloader, &FileDownloader::cancelled, this, &OnlineModDownloader::cancelled);
    connect(m_downloader, &FileDownloader::finished, this, &OnlineModDownloader::finished);
  }

  void download(const QString& sharingUrl, const QString& destinationFilePath) {
    m_cancelled = false;
    m_originalUrl = sharingUrl;
    m_destinationPath = destinationFilePath;

    if (m_currentReply) {
      m_currentReply->abort();
      m_currentReply->deleteLater();
      m_currentReply = nullptr;
    }

    if (sharingUrl.contains("drive.google.com")) {
      QRegularExpression rx("(?:/d/|[?&]id=)([a-zA-Z0-9_-]{25,})");
      const auto match = rx.match(sharingUrl);
      if (!match.hasMatch()) {
        emit failed("Invalid Google Drive URL. Could not find a valid File ID.");
        return;
      }
      const QString fileId = match.captured(1);
      const QUrl initialUrl(QString("https://drive.google.com/uc?export=download&id=%1").arg(fileId));
      requestInitialPage(initialUrl);
    } else if (sharingUrl.contains("mediafire.com")) {
      requestInitialPage(QUrl(sharingUrl));
    } else {
      checkDirectUrl(QUrl(sharingUrl));
    }
  }

  void cancel() {
    m_cancelled = true;
    if (m_currentReply) {
      m_currentReply->abort();
      m_currentReply->deleteLater();
      m_currentReply = nullptr;
    }
    m_downloader->cancel();
  }

signals:
  void progress(qint64 bytesReceived, qint64 bytesTotal);
  void finished(const QString& filePath);
  void failed(const QString& errorString);
  void cancelled();

private slots:
  void onInitialPageFinished() {
    if (!m_currentReply) return;
    QByteArray data = m_currentReply->readAll();
    const int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString contentType = m_currentReply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    const QUrl finalUrl = m_currentReply->url();
    if (m_currentReply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode < 600)) {
      const QString error = m_currentReply->errorString();
      m_currentReply->deleteLater();
      m_currentReply = nullptr;
      emit failed(QString("Failed to contact server landing page: %1").arg(error));
      return;
    }
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (m_originalUrl.contains("drive.google.com") && !contentType.contains("text/html")) {
      m_downloader->download(finalUrl, m_destinationPath);
    } else if (m_originalUrl.contains("mediafire.com")) {
      parseMediaFirePage(QString::fromUtf8(data));
    } else if (m_originalUrl.contains("drive.google.com")) {
      parseGoogleDrivePage(QString::fromUtf8(data));
    }
  }

  void onHeadFinished() {
    if (!m_currentReply) return;
    const int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl finalUrl = m_currentReply->url();
    if (m_currentReply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode < 600)) {
      const QString error = m_currentReply->errorString();
      m_currentReply->deleteLater();
      m_currentReply = nullptr;
      emit failed(QString("Direct URL validation failed: %1").arg(error));
      return;
    }
    const bool valid = isArchiveFile(finalUrl, m_currentReply);
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    if (valid) {
      m_downloader->download(finalUrl, m_destinationPath);
    } else {
      emit failed("Validation failed: The direct URL does not point to a supported archive file format.");
    }
  }

private:
  void requestInitialPage(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0");
    m_currentReply = m_networkManager.get(request);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &OnlineModDownloader::onInitialPageFinished);
  }

  void checkDirectUrl(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    m_currentReply = m_networkManager.head(request);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &OnlineModDownloader::onHeadFinished);
  }

  static bool isArchiveFile(const QUrl& url, QNetworkReply* headReply) {
    const QString path = url.path().toLower();
    static const QStringList extensions = {
      ".zip", ".rar", ".7z", ".tar", ".gz", ".bz2", ".xz",
      ".tgz", ".tbz2", ".zst", ".iso"
    };
    for (const QString& extension : extensions) {
      if (path.endsWith(extension)) return true;
    }
    if (!headReply) return false;
    const QString disposition =
      headReply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
    for (const QString& extension : extensions) {
      if (disposition.contains(extension)) return true;
    }
    const QString contentType =
      headReply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    return contentType.contains("zip") || contentType.contains("compressed");
  }

  void parseGoogleDrivePage(const QString& htmlContent) {
    const QByteArray utf8Data = htmlContent.toUtf8();
    lxb_html_document_t* document = lxb_html_document_create();
    if (!document) {
      emit failed("Failed to create Lexbor HTML document instance.");
      return;
    }
    if (lxb_html_document_parse(document, reinterpret_cast<const lxb_char_t*>(utf8Data.constData()),
                                utf8Data.size()) != LXB_STATUS_OK) {
      lxb_html_document_destroy(document);
      emit failed("Lexbor failed to parse Google Drive warning page HTML.");
      return;
    }
    lxb_dom_collection_t* collection =
      lxb_dom_collection_make(&document->dom_document, 16);
    if (!collection) {
      lxb_html_document_destroy(document);
      emit failed("Failed to allocate Lexbor DOM collection.");
      return;
    }

    lxb_dom_element_t* root = lxb_dom_interface_element(document->body);
    QString actionUrl;
    if (lxb_dom_elements_by_tag_name(root, collection,
                                     reinterpret_cast<const lxb_char_t*>("form"), 4) == LXB_STATUS_OK &&
        lxb_dom_collection_length(collection) > 0) {
      lxb_dom_element_t* form = lxb_dom_collection_element(collection, 0);
      size_t length = 0;
      const lxb_char_t* action =
        lxb_dom_element_get_attribute(form, reinterpret_cast<const lxb_char_t*>("action"), 6, &length);
      if (action) actionUrl = QString::fromUtf8(reinterpret_cast<const char*>(action), length);
    }
    lxb_dom_collection_clean(collection);
    if (actionUrl.isEmpty()) {
      lxb_dom_collection_destroy(collection, true);
      lxb_html_document_destroy(document);
      emit failed("Could not locate download form action URL via Lexbor parser.");
      return;
    }
    actionUrl.replace("&amp;", "&");
    if (actionUrl.startsWith("/")) actionUrl.prepend("https://drive.google.com");

    QUrl url(actionUrl);
    QUrlQuery query;
    if (lxb_dom_elements_by_tag_name(root, collection,
                                     reinterpret_cast<const lxb_char_t*>("input"), 5) == LXB_STATUS_OK) {
      const size_t length = lxb_dom_collection_length(collection);
      for (size_t i = 0; i < length; ++i) {
        lxb_dom_element_t* input = lxb_dom_collection_element(collection, i);
        size_t nameLength = 0, valueLength = 0, typeLength = 0;
        const lxb_char_t* typeValue =
          lxb_dom_element_get_attribute(input, reinterpret_cast<const lxb_char_t*>("type"), 4, &typeLength);
        const QString type = typeValue
          ? QString::fromUtf8(reinterpret_cast<const char*>(typeValue), typeLength).toLower()
          : QString();
        if (type == "hidden" || type == "submit" || type.isEmpty()) {
          const lxb_char_t* nameValue =
            lxb_dom_element_get_attribute(input, reinterpret_cast<const lxb_char_t*>("name"), 4, &nameLength);
          if (nameValue) {
            const QString name =
              QString::fromUtf8(reinterpret_cast<const char*>(nameValue), nameLength);
            const lxb_char_t* value =
              lxb_dom_element_get_attribute(input, reinterpret_cast<const lxb_char_t*>("value"), 5, &valueLength);
            const QString stringValue = value
              ? QString::fromUtf8(reinterpret_cast<const char*>(value), valueLength)
              : QString();
            query.addQueryItem(name, stringValue);
          }
        }
      }
    }
    lxb_dom_collection_destroy(collection, true);
    lxb_html_document_destroy(document);
    url.setQuery(query);
    m_downloader->download(url, m_destinationPath);
  }

  void parseMediaFirePage(const QString& htmlContent) {
    const QByteArray utf8Data = htmlContent.toUtf8();
    lxb_html_document_t* document = lxb_html_document_create();
    if (!document) {
      emit failed("Failed to create Lexbor HTML document instance for MediaFire.");
      return;
    }
    if (lxb_html_document_parse(document, reinterpret_cast<const lxb_char_t*>(utf8Data.constData()),
                                utf8Data.size()) != LXB_STATUS_OK) {
      lxb_html_document_destroy(document);
      emit failed("Lexbor failed to parse MediaFire landing page HTML.");
      return;
    }
    lxb_dom_collection_t* collection =
      lxb_dom_collection_make(&document->dom_document, 16);
    if (!collection) {
      lxb_html_document_destroy(document);
      emit failed("Failed to allocate Lexbor DOM collection for MediaFire.");
      return;
    }
    lxb_dom_element_t* root = lxb_dom_interface_element(document->body);
    QString downloadUrl;
    if (lxb_dom_elements_by_tag_name(root, collection,
                                     reinterpret_cast<const lxb_char_t*>("a"), 1) == LXB_STATUS_OK) {
      const size_t length = lxb_dom_collection_length(collection);
      for (size_t i = 0; i < length; ++i) {
        lxb_dom_element_t* element = lxb_dom_collection_element(collection, i);
        size_t idLength = 0;
        const lxb_char_t* idValue =
          lxb_dom_element_get_attribute(element, reinterpret_cast<const lxb_char_t*>("id"), 2, &idLength);
        if (idValue &&
            QString::fromUtf8(reinterpret_cast<const char*>(idValue), idLength) == "downloadButton") {
          size_t hrefLength = 0;
          const lxb_char_t* hrefValue =
            lxb_dom_element_get_attribute(element, reinterpret_cast<const lxb_char_t*>("href"), 4, &hrefLength);
          if (hrefValue) {
            downloadUrl =
              QString::fromUtf8(reinterpret_cast<const char*>(hrefValue), hrefLength);
          }
          break;
        }
      }
    }
    lxb_dom_collection_destroy(collection, true);
    lxb_html_document_destroy(document);
    if (downloadUrl.isEmpty()) {
      emit failed("Could not locate MediaFire direct download link via Lexbor parser.");
      return;
    }
    downloadUrl.replace("&amp;", "&");
    QTimer::singleShot(QRandomGenerator::global()->bounded(3000, 4000), this,
                       [this, downloadUrl]() {
      if (!m_cancelled) m_downloader->download(QUrl(downloadUrl), m_destinationPath);
    });
  }

  QNetworkAccessManager m_networkManager;
  QNetworkReply* m_currentReply = nullptr;
  FileDownloader* m_downloader;
  QString m_originalUrl;
  QString m_destinationPath;
  bool m_cancelled = false;
};

QString userReadableSize(qint64 bytes) {
  if (bytes < 1024) {
    return QString("%1 B").arg(bytes);
  } else if (bytes < 1024 * 1024) {
    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
  } else if (bytes < 1024 * 1024 * 1024) {
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
  } else {
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
  }
}

class DownloadDialogContent : public DialogContent {
  Q_OBJECT
private:
  OnlineMod m_mod;
  OnlineModDownloader* m_downloader = nullptr;
  QProgressBar* m_progressBar = nullptr;
  QLabel* m_statusLabel = nullptr;
  Button* m_cancelButton = nullptr;

  void finishTask() {
    setAllowClosing(true);
    if (m_cancelButton) {
      m_cancelButton->setEnabled(false);
    }
  }

  static QString downloadFileName(const OnlineMod& mod) {
    /*
    auto fileName = QFileInfo(QUrl(QString::fromStdString(mod.url)).path()).fileName();
    if (fileName.isEmpty()) {
      fileName = QString::fromStdString(mod.name).trimmed();
      fileName.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_");
      if (fileName.isEmpty()) {
        fileName = "mod-download";
      }
    }
    if (QFileInfo(fileName).suffix().isEmpty()) {
      fileName += ".zip";
    }
    */
    return QString::fromStdString(mod.name).replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_") + ".zip";
  }

public:
  explicit DownloadDialogContent(OnlineMod mod, QWidget* parent = nullptr)
    : DialogContent(parent), m_mod(std::move(mod)) {
    setAllowClosing(true);
    setFixedWidth(420);
    QPointer<DownloadDialogContent> self(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(25, 20, 25, 20);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("Downloading %1").arg(QString::fromStdString(m_mod.name)), this);
    titleLabel->setWordWrap(true);
    titleLabel->setFont(QFont("Quicksand", 12, QFont::Weight::Bold));
    layout->addWidget(titleLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(tr("Starting download..."), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_cancelButton = new Button(tr("Cancel"), this);
    m_cancelButton->setEnabled(true);
    layout->addWidget(m_cancelButton, 0, Qt::AlignHCenter);

    m_downloader = new OnlineModDownloader(this);
    auto onCancel = [self]() {
      if (!self || !self->m_downloader) return;
      self->m_downloader->cancel();
    };
    connect(m_cancelButton, &Button::clicked, this, onCancel);
    connect(dialog(), &Dialog::closed, this, onCancel);
    connect(m_downloader, &OnlineModDownloader::cancelled, this, [self]() {
      if (!self) return;
      self->m_statusLabel->setText(tr("Download cancelled."));
      self->finishTask();
    });
    // opens the mod url in the default browser if the download fails 
    // might not be ideal in a few cases though
    auto fallbackRoute = [self, modUrl = m_mod.url]() {
      if (!self) return;
      QDesktopServices::openUrl(QUrl(QString::fromStdString(modUrl)));
      self->finishTask();
    };
    connect(m_downloader, &OnlineModDownloader::progress, this,
            [self](qint64 bytesReceived, qint64 bytesTotal) {
              if (!self) return;
              if (bytesTotal > 0) {
                self->m_progressBar->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
                self->m_statusLabel->setText(tr("Downloading... %1 / %2")
                                         .arg(userReadableSize(bytesReceived))
                                         .arg(userReadableSize(bytesTotal)));
              } else {
                self->m_progressBar->setRange(0, 0);
                //m_statusLabel->setText(tr("Downloaded %1 bytes").arg(bytesReceived));
              }
            });
    connect(m_downloader, &OnlineModDownloader::finished, this,
            [self, fallbackRoute, modName = m_mod.name](const QString& filePath) {
      if (!self) return;
      self->m_cancelButton->setEnabled(false);
      self->setAllowClosing(false);
      self->m_statusLabel->setText(tr("Extracting mod..."));
      QThreadPool::globalInstance()->start([self, filePath, fallbackRoute, modName]() {
        const auto storagePath = QSettings().value(GSTK_STORAGE_PATH).toString();
        QDir storageDir(storagePath);
        if (storagePath.isEmpty() || !storageDir.mkpath(".")) {
          QMetaObject::invokeMethod(qApp, [self, fallbackRoute]() {
            if (!self) return;
            self->m_statusLabel->setText(tr("Could not access the mod storage path."));
            fallbackRoute();
          }, Qt::QueuedConnection);
          return;
        }
        // set extract folder to a subdirectory
        auto extractDir = QDir(storageDir.filePath(QString::fromStdString(modName).replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_")));
        std::string modDir = "";

        auto fp = filePath.toUtf8();

        // get the number of files in the archive
        // purely for progress bar purpose

        const int numFiles = countArchiveEntries(fp.constData());
        if (numFiles < 0) {
          QMetaObject::invokeMethod(qApp, [self]() {
            if (!self) return;
            self->m_statusLabel->setText(tr("Could not open the downloaded archive."));
            self->finishTask();
          }, Qt::QueuedConnection);
          return;
        }

        QMetaObject::invokeMethod(qApp, [self, numFiles]() {
          if (!self) return;
          self->m_progressBar->setRange(0, numFiles);
        }, Qt::QueuedConnection);

        std::string error;
        int extractedFiles = 0;
        const int result = extractArchive(
          fp.constData(), extractDir.path().toUtf8().constData(),
          [self, &modDir, &extractedFiles, &extractDir](const char* filename) {
          // if file name of fp is either "scripts.rpa" or "script.rpyc", 
          // set modDir to the grandparent folder
          std::string_view fpv(filename);
          if (fpv.ends_with("scripts.rpa") || fpv.ends_with("script.rpyc")) {
            std::filesystem::path fp(filename);
            modDir = extractDir.filePath(QString::fromStdString(fp.parent_path().parent_path().string())).toStdString();
          }
          extractedFiles++;
          const int progress = extractedFiles;
          QMetaObject::invokeMethod(qApp, [self, progress]() {
            if (self) self->m_progressBar->setValue(progress);
          }, Qt::QueuedConnection);
          }, error);
        if (result != 0) {
          QMetaObject::invokeMethod(qApp, [self, fallbackRoute, error]() {
            if (!self) return;
            self->m_statusLabel->setText(
              tr("Could not extract the downloaded mod: %1")
                  .arg(QString::fromStdString(error)));
            fallbackRoute();
          }, Qt::QueuedConnection);
          return;
        }

        // if modDir is still empty, the archive possibly 
        // does not contain a conventional renpy game packaging
        //
        // open the extracted folder and instruct user to install according to instructions if any

        if (modDir.empty()) {
          QMetaObject::invokeMethod(qApp, [self, extractDir]() {
            if (!self) return;
            self->m_statusLabel->setText(
              tr("The downloaded mod does not contain a recognizable Ren'Py game structure. Please check the extracted folder at %1 for installation instructions.")
                  .arg(extractDir.path()));
            QDesktopServices::openUrl(QUrl::fromLocalFile(extractDir.path())); 
            self->finishTask();
          }, Qt::QueuedConnection);
          return;
        }

        QFile::remove(filePath);
        QMetaObject::invokeMethod(qApp, [self]() {
          if (self) self->m_statusLabel->setText(tr("Installing..."));
        }, Qt::QueuedConnection);

        QMetaObject::invokeMethod(qApp, [self, fallbackRoute, modDir]() {
          if (!self) return;
          try {
            ModsIndex::get()->installMod(modDir);
            self->m_progressBar->setValue(100);
            self->m_statusLabel->setText(tr("Download complete."));
            self->finishTask();
            self->closeDialog();
          } catch (const std::exception& e) {
            self->m_statusLabel->setText(
              tr("Could not install the downloaded mod: %1")
                  .arg(QString::fromStdString(e.what())));
            fallbackRoute();
          }
        }, Qt::QueuedConnection);
      });
    });
    connect(m_downloader, &OnlineModDownloader::failed, this,
            [self, fallbackRoute](const QString& reason) {
        if (!self) return;
        self->m_statusLabel->setText(tr("Download failed. Please download & install the mod manually.\nReason: %1").arg(reason));
        fallbackRoute();
      });

    const auto storagePath = QSettings().value(GSTK_STORAGE_PATH).toString();
    if (storagePath.isEmpty()) {
      // this only happens if the user edits the configs file.
      // or maybe unreachable because it's reset to the default in main
      m_statusLabel->setText("Mod storage path is not set. Please set it in the settings.");
      finishTask();
      return;
    }
    const auto tempPath = QDir(storagePath).filePath("tempdl");
    if (!QDir().mkpath(tempPath)) {
      m_statusLabel->setText(tr("Could not create the temporary download directory. Please check the folder permission of %1").arg(tempPath));
      fallbackRoute();
      return;
    }

    m_downloader->download(
      QString::fromStdString(m_mod.url),
      QDir(tempPath).filePath(downloadFileName(m_mod))
    );
  }
};

class OnlineModInfoDC : public DialogContent {
  Q_OBJECT
private:
  OnlineMod m_mod;
  QNetworkAccessManager* m_nm = nullptr;
public:
  explicit OnlineModInfoDC(QNetworkAccessManager* networkManager, OnlineMod mod, QWidget* parent = nullptr)
    : DialogContent(parent), m_nm(networkManager), m_mod(mod) {
    setMinimumSize({800, 580});
    setMaximumSize({1720, 768});

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(12, 8, 8, 12);
    layout->setAlignment(Qt::AlignTop);


    auto* titleLabel = new ElidedLabel(QString::fromStdString(mod.name), this);
    titleLabel->setFont(QFont("Quicksand", 18, QFont::Weight::Bold));
    titleLabel->setMaxLines(2);

    auto* authorLabel = new QLabel(QString::fromStdString(mod.author), this);
    authorLabel->setFont(QFont("Quicksand", 15, QFont::Weight::DemiBold));

    auto* creationDateLineLayout = new QHBoxLayout();
    creationDateLineLayout->setContentsMargins(0, 0, 0, 0);
    creationDateLineLayout->setSpacing(4);
    creationDateLineLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    {
      auto* icon = new PixmapWidget(LucideIcons::calendar.pixmap({16,16}, c_secondaryColor));
      icon->setFixedSize({16,16});
      creationDateLineLayout->addWidget(icon);

      auto* creationDateLabel = new QLabel(
        QString::fromStdString(mod.creationDate.has_value() ? format_date(mod.creationDate.value()) : "Not provided")
      , this);
      creationDateLabel->setFont(QFont("Quicksand", 12));
      creationDateLineLayout->addWidget(creationDateLabel);
    }

    auto* metadata = new QWidget(this);
    auto* metadataLayout = new QVBoxLayout(metadata);
    metadataLayout->setContentsMargins(0, 0, 0, 0);
    metadataLayout->setSpacing(4);
    metadataLayout->addWidget(titleLabel);
    metadataLayout->addWidget(authorLabel);
    metadataLayout->addLayout(creationDateLineLayout);

    auto* copyDownloadURLButton = new IconButton(LucideIcons::link, this);
    connect(copyDownloadURLButton, &QPushButton::clicked, this, [mod, this]() {
      QClipboard* clipboard = QApplication::clipboard();
      clipboard->setText(QString::fromStdString(mod.url));
    });

    auto* downloadButton = new Button(tr("Download"), LucideIcons::download, this);
    connect(downloadButton, &QPushButton::clicked, this, [mod, this]() {
      Dialog::showContentDialog(
        dialog(),
        "Download mod",
        new DownloadDialogContent(mod)
      );
    });

    auto* metadataRow = new QHBoxLayout();
    metadataRow->setContentsMargins(0, 0, 0, 0);
    metadataRow->setSpacing(12);
    metadataRow->addWidget(metadata);
    metadataRow->addWidget(copyDownloadURLButton, 0, Qt::AlignVCenter);
    metadataRow->addWidget(downloadButton, 0, Qt::AlignVCenter);
    layout->addLayout(metadataRow);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* scrollContent = new QWidget();
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);



    std::vector<std::string> imageUrls;
    if (!mod.imageUrl.empty()) {
      imageUrls.push_back(mod.imageUrl);
    }
    imageUrls.insert(imageUrls.end(), mod.screenshotImageUrls.begin(), mod.screenshotImageUrls.end());
    if (imageUrls.empty()) {
      imageUrls.emplace_back();
    }

    QSize imageCarouselSize = {500, 281};

    auto* carouselLayout = new QHBoxLayout();
    carouselLayout->setContentsMargins(0, 0, 0, 0);
    carouselLayout->setSpacing(8);

    auto* previousImageButton = new IconButton(LucideIcons::arrow_left, this);
    auto* imageStack = new QStackedWidget(this);
    imageStack->setFixedSize(imageCarouselSize);
    std::vector<QWidget*> imageDots;
    for (const auto& imageUrl : imageUrls) {
      auto* image = new ImageLoadingWidget(QString::fromStdString(imageUrl), m_nm, imageStack);
      image->setFixedSize(imageCarouselSize);
      imageStack->addWidget(image);

      auto* dot = new QWidget(this);
      dot->setFixedSize(8, 8);
      imageDots.push_back(dot);
    }

    auto* nextImageButton = new IconButton(LucideIcons::arrow_right, this);
    carouselLayout->addWidget(previousImageButton);
    carouselLayout->addWidget(imageStack);
    carouselLayout->addWidget(nextImageButton);
    scrollLayout->addLayout(carouselLayout);

    auto* dotsLayout = new QHBoxLayout();
    dotsLayout->setContentsMargins(0, 4, 0, 0);
    dotsLayout->setSpacing(6);
    dotsLayout->setAlignment(Qt::AlignCenter);
    for (auto* dot : imageDots) {
      dotsLayout->addWidget(dot);
    }
    scrollLayout->addLayout(dotsLayout);

    auto updateCarousel = [imageStack, imageDots, previousImageButton, nextImageButton]() {
      const auto currentIndex = imageStack->currentIndex();
      previousImageButton->setEnabled(currentIndex > 0);
      nextImageButton->setEnabled(currentIndex + 1 < imageStack->count());
      for (int i = 0; i < static_cast<int>(imageDots.size()); ++i) {
        imageDots[i]->setStyleSheet(
          i == currentIndex
            ? "background-color: #ec50a3; border-radius: 4px;"
            : "background-color: #d0d0d0; border-radius: 4px;"
        );
      }
    };
    connect(previousImageButton, &QPushButton::clicked, this, [imageStack, updateCarousel]() {
      imageStack->setCurrentIndex(imageStack->currentIndex() - 1);
      updateCarousel();
    });
    connect(nextImageButton, &QPushButton::clicked, this, [imageStack, updateCarousel]() {
      imageStack->setCurrentIndex(imageStack->currentIndex() + 1);
      updateCarousel();
    });
    updateCarousel();

    scrollLayout->addSpacing(12);

    auto* descriptionLabel = new QLabel(QString::fromStdString(mod.description), scrollContent);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setFont(QFont("Quicksand", 12));
    scrollLayout->addWidget(descriptionLabel);
  }
};

class OMIThumbnail : public QWidget {
  Q_OBJECT
private:
  bool m_hovered = false;

  QNetworkAccessManager* m_nm = nullptr;

  ImageLoadingWidget* m_child = nullptr;
  
  const int m_padding = 8;
  const int m_radius = 4;
public:
  void setHovered(bool hovered){
    m_hovered = hovered;
    update();
  }

  void resizeEvent(QResizeEvent* event) override {
    updateChildRender();
  }
  void paintEvent(QPaintEvent* event) override {
    // Draw a rounded rectangle at `this->rect()` with *only* a 4px outline, 
    // using gradient color of c_primaryLightColor at the top-left and c_secondaryLightColor at the bottom-right

    if (m_hovered) {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing, true);

      QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
      gradient.setColorAt(0.0, c_primaryLightColor);
      gradient.setColorAt(1.0, c_secondaryLightColor);

      QPen pen(gradient, m_radius);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);

      // Inset by half the pen width (2px) so the 4px border stays inside the widget bounds
      painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 4, 4);
    }
  }
  void updateChildRender() {
    // Ensure padding doesn't result in negative dimensions if the window gets too small
    int childX = m_padding;
    int childY = m_padding;
    int childWidth = qMax(0, width() - (2 * m_padding));
    int childHeight = qMax(0, height() - (2 * m_padding));

    // Apply the padded geometry to the child
    m_child->setGeometry(childX, childY, childWidth, childHeight);

    QPainterPath path;
    path.addRoundedRect(m_child->rect(), m_radius, m_radius);
    QRegion roundedRegion(path.toFillPolygon().toPolygon());

    m_child->setMask(roundedRegion);
  }

  /*
   * @params displaySize image display size (including padding for the hover border)
   */
  explicit OMIThumbnail(QString imageUrl, QSize displaySize, QNetworkAccessManager* networkManager, QWidget* parent = nullptr)
    : QWidget(parent), m_nm(networkManager) {
    setFixedSize(displaySize);
    m_child = new ImageLoadingWidget(imageUrl, m_nm, this);

    updateChildRender();
  }
};

class OnlineModItem : public QWidget {
  Q_OBJECT
private:
  OnlineMod m_mod;
  QNetworkAccessManager* m_nm; // "Owned" by OnlineModsIndexScreen
  OMIThumbnail* m_thumbnail = nullptr;
public:
  explicit OnlineModItem(QNetworkAccessManager* networkManager, OnlineMod mod, QWidget* parent = nullptr)
    : QWidget(parent), m_nm(networkManager), m_mod(mod) {
    setFixedWidth(258);
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop);

    // OMIThumbnail (16:9 ratio), title and author
    m_thumbnail = new OMIThumbnail(QString::fromStdString(mod.imageUrl), QSize(250, 140), m_nm, this);
    layout->addWidget(m_thumbnail);

    auto* titleLabel = new ElidedLabel(QString::fromStdString(mod.name), this);
    //titleLabel->setWordWrap(true);
    titleLabel->setFont(QFont("Quicksand", 12, QFont::Weight::DemiBold));
    titleLabel->setMaxLines(2);
    layout->addWidget(titleLabel);

    auto* authorLabel = new QLabel(QString::fromStdString(mod.author), this);
    authorLabel->setFont(QFont("Quicksand", 10));
    layout->addWidget(authorLabel);

    for (auto* child : findChildren<QWidget*>()) {
      child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  }
  void enterEvent(QEnterEvent* event) override {
    m_thumbnail->setHovered(true);
    QWidget::enterEvent(event);
  }
  void leaveEvent(QEvent* event) override {
    m_thumbnail->setHovered(false);
    QWidget::leaveEvent(event);
  }
  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      Dialog::showContentDialog(
        getMainWindow(),
        tr("Mod information"),
        new OnlineModInfoDC(m_nm, m_mod)
      );
      event->accept();
      return;
    }
    QWidget::mouseReleaseEvent(event);
  }
};


OnlineModsIndexScreen::OnlineModsIndexScreen(Router* router, QWidget* parent) : QWidget(parent) {
  m_router = router;
  m_nm = createNetworkAccessManager(this);

  setStyleSheet(
    "#header {background-color: #fff7e7;} #footer {background-color: white;}"
  );

  setWindowTitle("Browse Mods");
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;

  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);
  layout->setAlignment(Qt::AlignTop);

  auto* header = new QWidget(this);
  header->setObjectName("header");
  header->setFixedHeight(40);
  header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(header);

  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(8, 0, 8, 0);
  headerLayout->setAlignment(Qt::AlignLeft);

  auto* backButton = new IconButton(LucideIcons::arrow_left);
  connect(backButton, &IconButton::clicked, [this](){
    m_router->navigate("/mods");
  });
  headerLayout->addWidget(backButton);

  auto* searchBar = new QLineEdit(header);
  addRetranslateCallback(this, [searchBar, this] { 
    searchBar->setPlaceholderText(tr("Search mods"));
  });
  searchBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  connect(searchBar, &QLineEdit::textEdited, this, [this](const QString& text){
    m_searchQuery = text.toStdString();
    m_page = 0;
    queryIndex();
  });
  headerLayout->addWidget(searchBar, 1);

  auto* modsCountLabel = new QLabel();
  headerLayout->addWidget(modsCountLabel);
  auto updateModsCountLabel = [this, modsCountLabel](){
    modsCountLabel->setText(QString("Mods: %1").arg(m_totalItems));
  };
  updateModsCountLabel();

  auto* indexSelector = new QComboBox(header);
  QList<QPointer<OnlineIndex>> indexes {{
    QPointer<OnlineIndex>(new DDMCIndex(this)),
    QPointer<OnlineIndex>(new DokiModsDotMeIndex(this))
  }}; 

  for (auto& index : indexes) {
    indexSelector->addItem(index->displayName());
  }

  connect(indexSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, indexes](int idx){
    if (idx < 0 || idx >= indexes.size()) return;
    auto index = indexes[idx];
    if (!index) return;
    m_currentIndex = index;
    queryIndex();
  });
  m_currentIndex = indexes[0];

  headerLayout->addWidget(indexSelector);

  auto* sortByLabel = new QLabel();
  addRetranslateCallback(this, [this, sortByLabel] {
    sortByLabel->setText(tr("Sort by"));
  });
  headerLayout->addWidget(sortByLabel);

  auto* sortSelector = new QComboBox(header);
  std::array<std::pair<ListSorting, std::string>, 5> sorts {{
    {ListSorting::Newest, QT_TR_NOOP("Newest")},
    {ListSorting::Oldest, QT_TR_NOOP("Oldest")},
    {ListSorting::TitleAZ, QT_TR_NOOP("Title A-Z")},
    {ListSorting::TitleZA, QT_TR_NOOP("Title Z-A")},
    {ListSorting::Author, QT_TR_NOOP("Author")},
  }};

  for (const auto& [sort, name] : sorts) {
    sortSelector->addItem(tr(name.c_str()));
  }
  addRetranslateCallback(this, [sortSelector, sorts]{
    int i = 0;
    for (const auto& [sort, name] : sorts) {
      sortSelector->setItemText(i, tr(name.c_str()));
      i++;
    }
  });

  connect(sortSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, sortSelector, sorts](int idx){
    if (idx < 0 || idx >= sorts.size()) return;
    auto sort = sorts[idx].first;
    m_sort = sort;
    queryIndex();
  });

  headerLayout->addWidget(sortSelector);

  
  m_container = new QStackedWidget(this);
  setupGradientWidget(m_container);
  m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(m_container);

  m_scrollArea = new QScrollArea(m_container);
  m_content = new QWidget();
  m_content->setObjectName("content");
  m_scrollArea->setWidget(m_content);
  m_scrollArea->setStyleSheet("QScrollArea { background: transparent; }");
  m_content->setStyleSheet("#content { background: transparent; }");
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_contentLayout = new FlowLayout(m_content);
  m_contentLayout->setSpacing(8);
  m_contentLayout->setAlignment(Qt::AlignHCenter);
  m_container->addWidget(m_scrollArea);


  m_placeholderWidget = new QWidget();
  auto* loadingCircle = new LoadingCircle(m_placeholderWidget);
  loadingCircle->setScale(2);
  loadingCircle->setColor(QColorConstants::Black);
  auto* pContentLayout = new QVBoxLayout(m_placeholderWidget);
  pContentLayout->addWidget(loadingCircle, 0, Qt::AlignCenter);
  m_container->addWidget(m_placeholderWidget);

  m_exceptionWidget = new QWidget();
  auto* exceptionLayout = new QVBoxLayout(m_exceptionWidget);
  m_exceptionLabel = new QLabel();
  m_exceptionLabel->setWordWrap(true);
  m_exceptionLabel->setAlignment(Qt::AlignCenter);
  exceptionLayout->addWidget(m_exceptionLabel, 0, Qt::AlignCenter);
  m_container->addWidget(m_exceptionWidget);


  auto* footer = new QWidget(this);
  footer->setObjectName("footer");
  footer->setFixedHeight(40);
  footer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(footer);
  m_footer = footer;

  auto* footerLayout = new QHBoxLayout(footer);
  footerLayout->setAlignment(Qt::AlignCenter);
  footerLayout->setContentsMargins(10, 0, 10, 0);
  footerLayout->setSpacing(8); 

  auto* firstPageBtn = new IconButton(LucideIcons::chevrons_left);
  connect(firstPageBtn, &IconButton::clicked, [this](){
      setPage(0);
  });
  footerLayout->addWidget(firstPageBtn);

  auto* prevPageBtn = new IconButton(LucideIcons::chevron_left);
  connect(prevPageBtn, &IconButton::clicked, [this](){
      if (m_page > 0) {
        setPage(m_page - 1);
      }
  });
  footerLayout->addWidget(prevPageBtn);

  footerLayout->addStretch(1);

  footerLayout->addWidget(new QLabel("Page", footer));

  auto* pageInput = new QLineEdit(footer);
  pageInput->setFixedWidth(40);
  pageInput->setAlignment(Qt::AlignCenter);
  pageInput->setText(QString::number(m_page + 1));
  connect(pageInput, &QLineEdit::editingFinished, [this, pageInput](){
      bool ok;
      int newPage = pageInput->text().toInt(&ok);
      if (ok && newPage > 0 && newPage <= m_totalPages) {
        setPage(newPage - 1); // Convert to 0-based index
      } else {
        pageInput->setText(QString::number(m_page + 1)); // Reset to current page
      }
  });
  footerLayout->addWidget(pageInput);

  auto* totalPagesLabel = new QLabel(footer); // text content set below
  footerLayout->addWidget(totalPagesLabel);

  auto* perPageInput = new QLineEdit(footer);
  perPageInput->setFixedWidth(40);
  perPageInput->setAlignment(Qt::AlignCenter);
  perPageInput->setText(QString::number(m_perPage));
  connect(perPageInput, &QLineEdit::editingFinished, [this, perPageInput](){
      bool ok;
      int newPerPage = perPageInput->text().toInt(&ok); 
      if (ok && newPerPage > 0 && newPerPage <= m_totalPages) {
        setPerPage(newPerPage);
      } else {
        perPageInput->setText(QString::number(m_perPage)); 
      }
  });
  footerLayout->addWidget(perPageInput);

  footerLayout->addWidget(new QLabel("mods per page", footer));
  footerLayout->addStretch(1);

  auto* nextPageBtn = new IconButton(LucideIcons::chevron_right);
  connect(nextPageBtn, &IconButton::clicked, [this](){
      if (m_page + 1 < m_totalPages) {
        setPage(m_page + 1);
      }
  });
  footerLayout->addWidget(nextPageBtn);

  auto* lastPageBtn = new IconButton(LucideIcons::chevrons_right);
  connect(lastPageBtn, &IconButton::clicked, [this](){
      if (m_page + 1 < m_totalPages) {
        setPage(m_totalPages - 1);
      }
  });
  footerLayout->addWidget(lastPageBtn);

  // tooltip for nav buttons
  addRetranslateCallback(this, [firstPageBtn, prevPageBtn, nextPageBtn, lastPageBtn]{
    firstPageBtn->setToolTip(tr("First page"));
    prevPageBtn->setToolTip(tr("Previous page"));
    nextPageBtn->setToolTip(tr("Next page"));
    lastPageBtn->setToolTip(tr("Last page"));
  });


  connect(this, &OnlineModsIndexScreen::pageNumberUpdated, [pageInput, firstPageBtn, prevPageBtn, nextPageBtn, lastPageBtn, this](){
    auto nv = m_page;
    pageInput->setText(QString::number(nv + 1));
    queryIndex();

    // disable navigation buttons based on the value
    firstPageBtn->setEnabled(nv > 0);
    prevPageBtn->setEnabled(nv > 0);
    nextPageBtn->setEnabled(nv + 1 < m_totalPages);
    lastPageBtn->setEnabled(nv + 1 < m_totalPages);
  });

  connect(this, &OnlineModsIndexScreen::perPageNumberUpdated, [this](){
    m_totalPages = std::max(1, (m_totalItems + m_perPage - 1) / m_perPage);
    setPage(m_page >= m_totalPages ? m_totalPages - 1 : m_page);
    queryIndex();
  });

  firstPageBtn->setEnabled(false);
  prevPageBtn->setEnabled(false);


  connect(this, &OnlineModsIndexScreen::totalItemsUpdated, [this, totalPagesLabel, updateModsCountLabel](){
    m_totalPages = std::max(1, (m_totalItems + m_perPage - 1) / m_perPage);
    totalPagesLabel->setText(QString("of %1, showing").arg(m_totalPages));
    updateModsCountLabel();
  });
}

void OnlineModsIndexScreen::queryIndex() {
  if (m_currentIndex != nullptr) {
    if (m_container) m_container->setCurrentWidget(m_placeholderWidget);
    if (m_footer) m_footer->hide();
    m_currentIndex->getMods({
      .page = m_page, .perPage = m_perPage, .sorting = m_sort,
      .searchQuery = m_searchQuery
    }).then(this, [this](std::vector<OnlineMod> list){
      for (auto* item : m_whar) {
        m_contentLayout->removeWidget(item);
        item->deleteLater();
      }
      m_whar.clear();

      for (const auto& mod : list) {
        auto* item = new OnlineModItem(m_nm, mod, m_content);
        m_contentLayout->addWidget(item);
        m_whar.push_back(item);
      }

      setTotalItems(m_currentIndex->getTotalMods({
        .sorting = m_sort,
        .searchQuery = m_searchQuery
      }));

      if (m_container) {
        m_scrollArea->verticalScrollBar()->setValue(0);
        m_container->setCurrentWidget(m_scrollArea);
      }
      if (m_footer && m_totalItems > m_perPage) m_footer->show();
    }).onFailed(this, [this](const std::exception& e){
      m_exceptionLabel->setText(QString("Failed to fetch mods: %1").arg(e.what()));
      if (m_container) m_container->setCurrentWidget(m_exceptionWidget);
    });
  }
}


void OnlineModsIndexScreen::showEvent(QShowEvent* event) {
  if (!m_firstShown) {
    queryIndex();
  }
  m_firstShown = true;
};

void OnlineModsIndexScreen::setTotalItems(int value) {
  if (value == m_totalItems) return;
  m_totalItems = value;
  emit totalItemsUpdated();
}
void OnlineModsIndexScreen::setPage(int page) {
  if (page == m_page) return;
  if (page >= 0 && page < m_totalPages) {
    m_page = page;
    emit pageNumberUpdated();
  }
}
void OnlineModsIndexScreen::setPerPage(int value) {
  if (value == m_perPage) return;
  // why 12?
  // why would you want anything smaller than this?
  if (value > 12 && value < m_totalItems) {
    m_perPage = value;
    emit perPageNumberUpdated();
  }
}

#include "OnlineModsIndex.moc"
