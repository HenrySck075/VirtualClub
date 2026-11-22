#include <archive.h>
#include <archive_entry.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>

#include <filesystem>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;

static QString argumentValue(const QStringList& arguments, const QString& name) {
  const int index = arguments.indexOf(name);
  return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString();
}

static bool isSafeArchivePath(const QString& name) {
  if (name.isEmpty() || name.startsWith('/') || name.contains("\\") ||
      name.contains(':') || name.contains("..")) {
    return false;
  }
  return true;
}

static bool extractArchive(const QString& archivePath, const QString& destination) {
  archive* input = archive_read_new();
  if (!input) return false;
  archive_read_support_filter_all(input);
  archive_read_support_format_all(input);
  if (archive_read_open_filename(input, archivePath.toUtf8().constData(), 10240) != ARCHIVE_OK) {
    std::cerr << archive_error_string(input) << std::endl;
    archive_read_free(input);
    return false;
  }

  fs::create_directories(destination.toStdString());
  archive_entry* entry = nullptr;
  int result = ARCHIVE_OK;
  while ((result = archive_read_next_header(input, &entry)) == ARCHIVE_OK) {
    const QString name = QString::fromUtf8(archive_entry_pathname(entry));
    if (!isSafeArchivePath(name)) {
      std::cerr << "Rejected unsafe archive path: " << name.toStdString() << std::endl;
      archive_read_free(input);
      return false;
    }

    const fs::path output = fs::path(destination.toStdString()) / name.toStdString();
    const auto type = archive_entry_filetype(entry);
    if (type == AE_IFDIR) {
      fs::create_directories(output);
      continue;
    }
    if (type == AE_IFLNK) {
      const QString target = QString::fromUtf8(archive_entry_symlink(entry));
      if (target.isEmpty() || target.startsWith('/') || target.contains("..")) {
        archive_read_free(input);
        return false;
      }
      fs::create_directories(output.parent_path());
      fs::remove(output);
      fs::create_symlink(target.toStdString(), output);
      continue;
    }
    if (type != AE_IFREG) continue;

    fs::create_directories(output.parent_path());
    QFile file(QString::fromStdString(output.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      archive_read_free(input);
      return false;
    }
    const void* buffer = nullptr;
    std::size_t size = 0;
    std::int64_t offset = 0;
    while (archive_read_data_block(input, &buffer, &size, &offset) == ARCHIVE_OK) {
      if (size > 0 && file.write(static_cast<const char*>(buffer), static_cast<qint64>(size)) != static_cast<qint64>(size)) {
        file.close();
        archive_read_free(input);
        return false;
      }
    }
    file.close();
  }

  if (result != ARCHIVE_EOF) {
    std::cerr << archive_error_string(input) << std::endl;
    archive_read_free(input);
    return false;
  }
  return archive_read_free(input) == ARCHIVE_OK;
}

static void mergeDirectory(const fs::path& source, const fs::path& destination) {
  if (!fs::exists(source)) return;
  fs::create_directories(destination);
  for (const auto& entry : fs::recursive_directory_iterator(source)) {
    const fs::path relative = fs::relative(entry.path(), source);
    const fs::path target = destination / relative;
    if (entry.is_directory()) {
      fs::create_directories(target);
    } else if (entry.is_symlink()) {
      fs::remove(target);
      fs::create_symlink(fs::read_symlink(entry.path()), target);
    } else if (entry.is_regular_file()) {
      fs::create_directories(target.parent_path());
      fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
    }
  }
}

static bool processIsRunning(qint64 pid) {
#ifdef Q_OS_WIN
  QProcess process;
  process.start("tasklist", {"/FI", QString("PID eq %1").arg(pid), "/FO", "CSV", "/NH"});
  process.waitForFinished(3000);
  const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
  return output.contains(QString::number(pid));
#else
  return QFileInfo::exists("/proc/" + QString::number(pid));
#endif
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  const QStringList arguments = app.arguments();

  const QString archivePath = argumentValue(arguments, "--archive");
  const QString installDir = argumentValue(arguments, "--install-dir");
  const QString appExe = argumentValue(arguments, "--app");
  const qint64 waitPid = argumentValue(arguments, "--wait-pid").toLongLong();
  if (archivePath.isEmpty() || installDir.isEmpty() || appExe.isEmpty() || waitPid <= 0) {
    return 2;
  }

  for (int i = 0; i < 600 && processIsRunning(waitPid); ++i) {
    QThread::msleep(100);
  }
  if (processIsRunning(waitPid)) return 3;

  const fs::path install(installDir.toStdString());
  fs::path parent = install.parent_path();
  fs::path staging = parent /
      ("VirtualClub-update-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::path rollback = parent /
      ("VirtualClub-rollback-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::remove_all(staging);
  fs::remove_all(rollback);

  if (!extractArchive(archivePath, QString::fromStdString(staging.string()))) return 4;

  const fs::path oldAssets = install / "assets";
  const fs::path newAssets = staging / "assets";
  if (fs::exists(oldAssets)) {
    mergeDirectory(oldAssets, newAssets);
  }

  std::error_code error;
  fs::rename(install, rollback, error);
  if (error) return 5;
  fs::rename(staging, install, error);
  if (error) {
    fs::rename(rollback, install);
    return 6;
  }

  fs::remove_all(rollback, error);
  fs::remove(archivePath.toStdString(), error);

  QProcess::startDetached(appExe, QStringList());
  return 0;
}
