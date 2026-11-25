#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <shared_mutex>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace vfs {

enum class Status {
  Ok,
  NoSuchEntry,
  EntryExists,
  NotDirectory,
  IsDirectory,
  PermissionDenied,
  DirectoryNotEmpty,
  OperationNotPermitted,
  NameTooLong,
  NoSpace,
  ReadOnly,
  InvalidArgument,
  IoError,
  NoData,
  Range,
};

struct Metadata {
  std::uint64_t size = 0;
  bool isDirectory = false;
  bool isReadOnly = false;
  std::uint32_t permissions = 0;
  std::uint64_t linkCount = 1;
  std::int64_t accessTime = 0;
  std::int64_t modificationTime = 0;
  std::int64_t changeTime = 0;
};

struct DirectoryEntry {
  std::string name;
  Metadata metadata;
};

struct OpenOptions {
  bool read = false;
  bool write = false;
  bool create = false;
  bool truncate = false;
  bool append = false;
};

struct StatFs {
  std::uint64_t blockSize = 4096;
  std::uint64_t fragmentSize = 4096;
  std::uint64_t blocks = 0;
  std::uint64_t freeBlocks = 0;
  std::uint64_t availableBlocks = 0;
};

struct HandleContext {
  std::uint64_t id = 0;
  std::string resolvedPath;
  bool openedForWrite = false;
  bool pendingDelete = false;
};

Status statusFromErrorCode(const std::error_code& error);

class VFSCoreOperations {
public:
  virtual ~VFSCoreOperations() = default;

  virtual Status getMetadata(const std::string& path, Metadata& metadata) const = 0;
  virtual Status readDirectory(const std::string& path,
                               std::vector<DirectoryEntry>& entries) const = 0;
  virtual Status openFile(const std::string& path,
                          const OpenOptions& options,
                          HandleContext& handle) = 0;
  virtual Status createFile(const std::string& path,
                           const OpenOptions& options,
                           HandleContext& handle) = 0;
  virtual Status createDirectory(const std::string& path) = 0;
  virtual Status read(std::uint64_t handleId,
                      std::uint64_t offset,
                      std::span<std::byte> buffer,
                      std::size_t& bytesRead) = 0;
  virtual Status write(std::uint64_t handleId,
                       std::uint64_t offset,
                       std::span<const std::byte> buffer,
                       std::size_t& bytesWritten) = 0;
  virtual Status setPendingDelete(std::uint64_t handleId, bool pending) = 0;
  virtual Status close(std::uint64_t handleId) = 0;
  virtual Status unlink(const std::string& path) = 0;
  virtual Status removeDirectory(const std::string& path) = 0;
  virtual Status rename(const std::string& oldPath, const std::string& newPath) = 0;
  virtual Status setAttributes(const std::string& path, const Metadata& metadata) = 0;
  virtual Status setFileSize(const std::string& path, std::uint64_t size) = 0;
  virtual Status access(const std::string& path, int mode) const = 0;
  virtual Status getExtendedAttribute(const std::string& path,
                                      const std::string& name,
                                      char* value,
                                      std::size_t size,
                                      std::size_t& valueSize) const = 0;
  virtual Status readLink(const std::string& path, std::string& target) const = 0;
  virtual Status statfs(const std::string& path, StatFs& stats) const = 0;
};

std::unique_ptr<VFSCoreOperations> createCore(std::string launcherRoot,
                                               std::string modId,
                                               std::string modPath);

class WhiteoutStore {
public:
  explicit WhiteoutStore(std::filesystem::path filePath);

  void load();
  bool contains(const std::string& path) const;
  Status add(const std::string& path);
  Status remove(const std::string& path);

private:
  std::filesystem::path filePath;
  std::unordered_set<std::string> paths;
  mutable std::shared_mutex mutex;

  static std::string normalizePath(const std::string& path);
  Status saveLocked();
};

} // namespace vfs
