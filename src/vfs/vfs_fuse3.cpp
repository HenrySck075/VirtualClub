#include "utils/macros.h"
#include "vfs.hpp"

#include <stdexcept>

#if defined(MVC_VFS_AVAILABLE) && defined(Q_OS_LINUX)
#include "vfs_core.hpp"

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <QDebug>
#include <QDir>
#include <QTemporaryDir>

namespace {

int fuseStatus(vfs::Status status) {
  switch (status) {
  case vfs::Status::Ok: return 0;
  case vfs::Status::NoSuchEntry: return -ENOENT;
  case vfs::Status::EntryExists: return -EEXIST;
  case vfs::Status::NotDirectory: return -ENOTDIR;
  case vfs::Status::IsDirectory: return -EISDIR;
  case vfs::Status::PermissionDenied: return -EACCES;
  case vfs::Status::DirectoryNotEmpty: return -ENOTEMPTY;
  case vfs::Status::OperationNotPermitted: return -EPERM;
  case vfs::Status::NameTooLong: return -ENAMETOOLONG;
  case vfs::Status::NoSpace: return -ENOSPC;
  case vfs::Status::ReadOnly: return -EROFS;
  case vfs::Status::InvalidArgument: return -EINVAL;
  case vfs::Status::NoData: return -ENODATA;
  case vfs::Status::Range: return -ERANGE;
  case vfs::Status::IoError: return -EIO;
  }
  return -EIO;
}

vfs::VFSCoreOperations* core() {
  const auto* context = fuse_get_context();
  return context ? static_cast<vfs::VFSCoreOperations*>(context->private_data) : nullptr;
}

vfs::OpenOptions openOptions(int flags) {
  vfs::OpenOptions options;
  options.read = (flags & O_ACCMODE) != O_WRONLY;
  options.write = (flags & O_ACCMODE) != O_RDONLY ||
                  (flags & (O_CREAT | O_TRUNC | O_APPEND)) != 0;
  options.create = (flags & O_CREAT) != 0;
  options.truncate = (flags & O_TRUNC) != 0;
  options.append = (flags & O_APPEND) != 0;
  return options;
}

int getattrCallback(const char* path, struct stat* output, struct fuse_file_info*) {
  auto* operations = core();
  if (!operations) return -EIO;
  vfs::Metadata metadata;
  const int result = fuseStatus(operations->getMetadata(path, metadata));
  if (result != 0) return result;

  std::memset(output, 0, sizeof(*output));
  output->st_mode = (metadata.isDirectory ? S_IFDIR : S_IFREG) |
                    static_cast<mode_t>(metadata.permissions & 0777U);
  output->st_nlink = static_cast<nlink_t>(metadata.linkCount);
  output->st_size = static_cast<off_t>(metadata.size);
  output->st_uid = getuid();
  output->st_gid = getgid();
  output->st_atim.tv_sec = static_cast<time_t>(metadata.accessTime);
  output->st_mtim.tv_sec = static_cast<time_t>(metadata.modificationTime);
  output->st_ctim.tv_sec = static_cast<time_t>(metadata.changeTime);
  return 0;
}

int readdirCallback(const char* path, void* buffer, fuse_fill_dir_t filler,
                    off_t, struct fuse_file_info*, enum fuse_readdir_flags) {
  auto* operations = core();
  if (!operations) return -EIO;
  std::vector<vfs::DirectoryEntry> entries;
  const int result = fuseStatus(operations->readDirectory(path, entries));
  if (result != 0) return result;
  for (const auto& entry : entries) {
    struct stat metadata {};
    metadata.st_mode = (entry.metadata.isDirectory ? S_IFDIR : S_IFREG) |
                       static_cast<mode_t>(entry.metadata.permissions & 0777U);
    if (filler(buffer, entry.name.c_str(), &metadata, 0,
               static_cast<fuse_fill_dir_flags>(0)) != 0) break;
  }
  return 0;
}

int openCallback(const char* path, struct fuse_file_info* info) {
  auto* operations = core();
  if (!operations) return -EIO;
  vfs::HandleContext handle;
  const int result = fuseStatus(operations->openFile(path, openOptions(info->flags), handle));
  if (result == 0) info->fh = handle.id;
  return result;
}

int createCallback(const char* path, mode_t mode, struct fuse_file_info* info) {
  auto* operations = core();
  if (!operations) return -EIO;
  vfs::HandleContext handle;
  vfs::OpenOptions options = openOptions(info->flags);
  options.create = true;
  options.write = true;
  const int result = fuseStatus(operations->createFile(path, options, handle));
  if (result == 0) {
    info->fh = handle.id;
    vfs::Metadata metadata;
    const int metadataResult = fuseStatus(operations->getMetadata(path, metadata));
    if (metadataResult != 0) {
      operations->close(handle.id);
      return metadataResult;
    }
    metadata.permissions = static_cast<std::uint32_t>(mode) & 0777U;
    metadata.isReadOnly = (metadata.permissions & 0222U) == 0;
    const int attributeResult = fuseStatus(operations->setAttributes(path, metadata));
    if (attributeResult != 0) {
      operations->close(handle.id);
      return attributeResult;
    }
  }
  return result;
}

int readCallback(const char*, char* buffer, size_t size, off_t offset,
                 struct fuse_file_info* info) {
  if (offset < 0) return -EINVAL;
  auto* operations = core();
  if (!operations) return -EIO;
  std::size_t count = 0;
  auto bytes = std::span(reinterpret_cast<std::byte*>(buffer), size);
  const int result = fuseStatus(operations->read(info->fh, static_cast<std::uint64_t>(offset),
                                                  bytes, count));
  if (result != 0) return result;
  if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) return -EOVERFLOW;
  return static_cast<int>(count);
}

int writeCallback(const char*, const char* buffer, size_t size, off_t offset,
                  struct fuse_file_info* info) {
  if (offset < 0) return -EINVAL;
  auto* operations = core();
  if (!operations) return -EIO;
  std::size_t count = 0;
  auto bytes = std::span(reinterpret_cast<const std::byte*>(buffer), size);
  const int result = fuseStatus(operations->write(info->fh, static_cast<std::uint64_t>(offset),
                                                   bytes, count));
  if (result != 0) return result;
  if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) return -EOVERFLOW;
  return static_cast<int>(count);
}

int releaseCallback(const char*, struct fuse_file_info* info) {
  auto* operations = core();
  return operations ? fuseStatus(operations->close(info->fh)) : -EIO;
}

int readlinkCallback(const char* path, char* buffer, size_t size) {
  auto* operations = core();
  if (!operations) return -EIO;
  std::string target;
  const int result = fuseStatus(operations->readLink(path, target));
  if (result != 0) return result;
  if (size == 0) return -ERANGE;
  const size_t count = (std::min)(size - 1, target.size());
  std::memcpy(buffer, target.data(), count);
  buffer[count] = '\0';
  return 0;
}

int accessCallback(const char* path, int mode) {
  auto* operations = core();
  return operations ? fuseStatus(operations->access(path, mode)) : -EIO;
}

int statfsCallback(const char* path, struct statvfs* output) {
  auto* operations = core();
  if (!operations) return -EIO;
  vfs::StatFs stats;
  const int result = fuseStatus(operations->statfs(path, stats));
  if (result != 0) return result;
  std::memset(output, 0, sizeof(*output));
  output->f_bsize = stats.blockSize;
  output->f_frsize = stats.fragmentSize;
  output->f_blocks = stats.blocks;
  output->f_bfree = stats.freeBlocks;
  output->f_bavail = stats.availableBlocks;
  return 0;
}

int truncateCallback(const char* path, off_t size, struct fuse_file_info*) {
  if (size < 0) return -EINVAL;
  auto* operations = core();
  return operations ? fuseStatus(operations->setFileSize(path, size)) : -EIO;
}

int mkdirCallback(const char* path, mode_t mode) {
  auto* operations = core();
  if (!operations) return -EIO;
  const int result = fuseStatus(operations->createDirectory(path));
  if (result != 0) return result;
  vfs::Metadata metadata;
  const int metadataResult = fuseStatus(operations->getMetadata(path, metadata));
  if (metadataResult != 0) return metadataResult;
  metadata.permissions = static_cast<std::uint32_t>(mode) & 0777U;
  metadata.isReadOnly = (metadata.permissions & 0222U) == 0;
  return fuseStatus(operations->setAttributes(path, metadata));
}

int unlinkCallback(const char* path) {
  auto* operations = core();
  return operations ? fuseStatus(operations->unlink(path)) : -EIO;
}

int rmdirCallback(const char* path) {
  auto* operations = core();
  return operations ? fuseStatus(operations->removeDirectory(path)) : -EIO;
}

int renameCallback(const char* oldPath, const char* newPath, unsigned int) {
  auto* operations = core();
  return operations ? fuseStatus(operations->rename(oldPath, newPath)) : -EIO;
}

int chmodCallback(const char* path, mode_t mode, struct fuse_file_info*) {
  auto* operations = core();
  if (!operations) return -EIO;
  vfs::Metadata metadata;
  const int result = fuseStatus(operations->getMetadata(path, metadata));
  if (result != 0) return result;
  metadata.permissions = static_cast<std::uint32_t>(mode) & 0777U;
  metadata.isReadOnly = (metadata.permissions & 0222U) == 0;
  return fuseStatus(operations->setAttributes(path, metadata));
}

int utimensCallback(const char* path, const struct timespec times[2],
                    struct fuse_file_info*) {
  auto* operations = core();
  if (!operations) return -EIO;
  vfs::Metadata metadata;
  const int result = fuseStatus(operations->getMetadata(path, metadata));
  if (result != 0) return result;
  if (times) {
    if (times[1].tv_nsec != UTIME_OMIT) {
      metadata.modificationTime = times[1].tv_nsec == UTIME_NOW
        ? std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
        : times[1].tv_sec;
    }
  } else {
    metadata.modificationTime = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  }
  return fuseStatus(operations->setAttributes(path, metadata));
}

int getxattrCallback(const char* path, const char* name, char* value, size_t size) {
  auto* operations = core();
  if (!operations) return -EIO;
  std::size_t valueSize = 0;
  const int result = fuseStatus(operations->getExtendedAttribute(path, name, value, size,
                                                                  valueSize));
  if (result != 0) return result;
  if (valueSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) return -EOVERFLOW;
  return static_cast<int>(valueSize);
}

void* initCallback(struct fuse_conn_info*, struct fuse_config*) {
  return core();
}

struct fuse_operations fuseOperations() {
  struct fuse_operations operations {};
  operations.getattr = getattrCallback;
  operations.readdir = readdirCallback;
  operations.open = openCallback;
  operations.create = createCallback;
  operations.read = readCallback;
  operations.write = writeCallback;
  operations.release = releaseCallback;
  operations.readlink = readlinkCallback;
  operations.access = accessCallback;
  operations.statfs = statfsCallback;
  operations.truncate = truncateCallback;
  operations.mkdir = mkdirCallback;
  operations.rmdir = rmdirCallback;
  operations.unlink = unlinkCallback;
  operations.rename = renameCallback;
  operations.chmod = chmodCallback;
  operations.utimens = utimensCallback;
  operations.getxattr = getxattrCallback;
  operations.init = initCallback;
  return operations;
}

} // namespace

class VFSInstance::Private {
public:
  Private(const QString& launcherRoot, const ModsIndex::Mod& mod)
      : mountpoint_(createMountPoint(QString::fromStdString(mod.id))),
        core_(vfs::createCore(launcherRoot.toStdString(), mod.id, mod.modPath)) {}

  ~Private() { unmount(); }

  bool mount() {
    std::lock_guard lock(mutex_);
    if (used_ || filesystem_ || loopThread_.joinable()) return false;
    used_ = true;

    struct fuse_operations operations = fuseOperations();
    struct fuse_args arguments = FUSE_ARGS_INIT(0, nullptr);
    fuse_opt_add_arg(&arguments, "vclubmgr_vfs");
    fuse_opt_add_arg(&arguments, "-o");
    fuse_opt_add_arg(&arguments, "kernel_cache,attr_timeout=10,entry_timeout=10,negative_timeout=2");
    filesystem_ = fuse_new(&arguments, &operations, sizeof(operations), core_.get());
    fuse_opt_free_args(&arguments);
    if (!filesystem_) return false;

    if (fuse_set_signal_handlers(fuse_get_session(filesystem_)) != 0) {
      qCritical() << "Failed to install FUSE signal handlers";
    }
    if (fuse_mount(filesystem_, mountpoint_.toStdString().c_str()) != 0) {
      qCritical() << "Failed to mount FUSE filesystem at" << mountpoint_;
      fuse_remove_signal_handlers(fuse_get_session(filesystem_));
      fuse_destroy(filesystem_);
      filesystem_ = nullptr;
      return false;
    }

    loopThread_ = std::thread([this] { fuse_loop_mt(filesystem_, 0); });
    return true;
  }

  void unmount() {
    std::unique_lock lock(mutex_);
    if (!filesystem_ && !loopThread_.joinable()) {
      cleanupMountpoint();
      return;
    }

    struct fuse* filesystem = filesystem_;
    struct fuse_session* session = filesystem ? fuse_get_session(filesystem) : nullptr;
    std::thread loop = std::move(loopThread_);
    filesystem_ = nullptr;
    lock.unlock();

    if (session) {
      fuse_exit(filesystem);
      fuse_unmount(filesystem);
    }
    if (loop.joinable()) loop.join();
    if (filesystem) {
      fuse_remove_signal_handlers(session);
      fuse_destroy(filesystem);
    }
    cleanupMountpoint();
  }

  QString mountPath() const { return mountpoint_; }

private:
  static QString createMountPoint(const QString& modId) {
    QTemporaryDir directory(QDir::tempPath() + "/vclubmgr_mount_" + modId + "_XXXXXX");
    if (!directory.isValid()) {
      throw std::runtime_error("Failed to create temporary mount point");
    }
    const QString path = directory.path();
    directory.setAutoRemove(false);
    return path;
  }

  void cleanupMountpoint() {
    if (mountpoint_.isEmpty()) return;
    std::error_code error;
    std::filesystem::remove_all(mountpoint_.toStdString(), error);
    if (error) {
      qWarning() << "Failed to remove mount point:" << QString::fromStdString(error.message());
    }
  }

  const QString mountpoint_;
  std::unique_ptr<vfs::VFSCoreOperations> core_;
  struct fuse* filesystem_ = nullptr;
  std::thread loopThread_;
  bool used_ = false;
  std::mutex mutex_;
};

VFSInstance::VFSInstance(const QString& launcherRoot, const ModsIndex::Mod& mod)
    : d(std::make_unique<Private>(launcherRoot, mod)) {}

VFSInstance::~VFSInstance() = default;

bool VFSInstance::mount() { return d->mount(); }
void VFSInstance::unmount() { d->unmount(); }
QString VFSInstance::mountPath() const { return d->mountPath(); }
#else
VFSInstance::VFSInstance(const QString&, const ModsIndex::Mod&) {
  throw std::runtime_error("VFS is not available on this platform.");
}

VFSInstance::~VFSInstance() = default;

bool VFSInstance::mount() { return false; }
void VFSInstance::unmount() {}
QString VFSInstance::mountPath() const { return {}; }
#endif
