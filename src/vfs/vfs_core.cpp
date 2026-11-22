#include "vfs_core.hpp"

#include "VFSPathResolver.hpp"
#include "consts.hpp"
#include "utils/ModIndex.hpp"
#include "utils/ProfileSettings.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <QSettings>
#include <QStandardPaths>
#include <utility>

namespace fs = std::filesystem;

namespace vfs {

Status statusFromErrorCode(const std::error_code& error) {
  if (!error) return Status::Ok;
  if (error == std::errc::no_such_file_or_directory) return Status::NoSuchEntry;
  if (error == std::errc::file_exists) return Status::EntryExists;
  if (error == std::errc::not_a_directory) return Status::NotDirectory;
  if (error == std::errc::is_a_directory) return Status::IsDirectory;
  if (error == std::errc::permission_denied) return Status::PermissionDenied;
  if (error == std::errc::directory_not_empty) return Status::DirectoryNotEmpty;
  if (error == std::errc::operation_not_permitted) return Status::OperationNotPermitted;
  if (error == std::errc::filename_too_long) return Status::NameTooLong;
  if (error == std::errc::no_space_on_device) return Status::NoSpace;
  if (error == std::errc::read_only_file_system) return Status::ReadOnly;
  if (error == std::errc::invalid_argument) return Status::InvalidArgument;
  return Status::IoError;
}

WhiteoutStore::WhiteoutStore(fs::path filePath) : filePath(std::move(filePath)) {}

std::string WhiteoutStore::normalizePath(const std::string& path) {
  fs::path normalized = fs::path(path).relative_path().lexically_normal();
  while (!normalized.empty() && normalized.filename().empty()) {
    normalized = normalized.parent_path();
  }
  if (normalized == ".") normalized.clear();
  return (fs::path("/") / normalized).generic_string();
}

void WhiteoutStore::load() {
  std::unique_lock lock(mutex);
  paths.clear();

  std::error_code error;
  if (!fs::exists(filePath, error)) {
    if (error) throw std::system_error(error, "Failed to check whiteout file");
    return;
  }

  std::ifstream input(filePath);
  if (!input) {
    throw std::system_error(std::make_error_code(std::errc::io_error),
                            "Failed to open whiteout file");
  }

  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty()) paths.insert(normalizePath(line));
  }
  if (input.bad()) {
    throw std::system_error(std::make_error_code(std::errc::io_error),
                            "Failed to read whiteout file");
  }
}

bool WhiteoutStore::contains(const std::string& path) const {
  const std::string normalized = normalizePath(path);
  std::shared_lock lock(mutex);
  if (paths.contains(normalized)) return true;

  fs::path parent = normalized;
  while (parent.has_parent_path()) {
    parent = parent.parent_path();
    if (parent.empty() || parent == "/") break;
    if (paths.contains(parent.generic_string())) return true;
  }
  return false;
}

Status WhiteoutStore::add(const std::string& path) {
  std::unique_lock lock(mutex);
  paths.insert(normalizePath(path));
  return saveLocked();
}

Status WhiteoutStore::remove(const std::string& path) {
  std::unique_lock lock(mutex);
  paths.erase(normalizePath(path));
  return saveLocked();
}

Status WhiteoutStore::saveLocked() {
  std::error_code error;
  fs::create_directories(filePath.parent_path(), error);
  if (error) return statusFromErrorCode(error);

  std::ofstream output(filePath, std::ios::trunc);
  if (!output) return Status::IoError;
  for (const auto& path : paths) output << path << '\n';
  return output ? Status::Ok : Status::IoError;
}

namespace {

Status filesystemStatus(const std::error_code& error) {
  return statusFromErrorCode(error);
}

std::int64_t toUnixSeconds(fs::file_time_type value) {
  const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
    value - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
  return std::chrono::duration_cast<std::chrono::seconds>(
    systemTime.time_since_epoch()).count();
}

fs::file_time_type fromUnixSeconds(std::int64_t value) {
  const auto systemTime = std::chrono::system_clock::time_point{
    std::chrono::seconds{value}};
  return std::chrono::time_point_cast<fs::file_time_type::duration>(
    systemTime - std::chrono::system_clock::now() + fs::file_time_type::clock::now());
}

class FilesystemCore final : public VFSCoreOperations {
public:
  FilesystemCore(std::string launcherRoot, std::string modId, std::string modPath)
      : launcherRoot_(std::move(launcherRoot)), modFolder_(std::move(modPath)) {
    auto settings = ProfileSettings::get();
    baseFolder_ = settings->value(STK_BASEPATH).toString().toStdString();
    if (baseFolder_.empty()) {
      throw std::runtime_error("Base game installation path not found.");
    }

    std::error_code error;
    containsRenpyEngine_ = fs::exists(modFolder_ / "lib", error);
    if (error) throw std::system_error(error, "Failed to check mod engine directory");
    const fs::path dataFolder =
      fs::path(QSettings().value(GSTK_STORAGE_PATH).toString().toStdString()) /
      "session_data";
    deltaFolder_ = dataFolder / "delta" / modId;
    contentFolder_ = deltaFolder_ / "content";
    fs::create_directories(contentFolder_, error);
    if (error) throw std::system_error(error, "Failed to create mod delta directory");
    whiteouts_ = std::make_unique<WhiteoutStore>(deltaFolder_ / ".whiteouts");
    whiteouts_->load();
    resolver_ = std::make_unique<VFSPathResolver>(
      baseFolder_, modFolder_, deltaFolder_, launcherRoot_, std::move(modId),
      containsRenpyEngine_);
  }

  Status getMetadata(const std::string& path, Metadata& metadata) const override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    const fs::path resolved = resolve(path);
    std::error_code error;
    const auto status = fs::status(resolved, error);
    if (error) return filesystemStatus(error);
    if (!fs::exists(status)) return Status::NoSuchEntry;

    metadata = {};
    metadata.isDirectory = fs::is_directory(status);
    if (!metadata.isDirectory) {
      metadata.size = fs::file_size(resolved, error);
      if (error) return filesystemStatus(error);
    }
    const auto perms = status.permissions();
    metadata.isReadOnly = (perms & (fs::perms::owner_write | fs::perms::group_write |
                                    fs::perms::others_write)) == fs::perms::none;
    metadata.permissions = static_cast<std::uint32_t>(perms) & 0777U;
    metadata.linkCount = 1;
    const auto modified = fs::last_write_time(resolved, error);
    if (!error) {
      metadata.modificationTime = toUnixSeconds(modified);
      metadata.accessTime = metadata.changeTime = metadata.modificationTime;
    }
    return Status::Ok;
  }

  Status readDirectory(const std::string& path,
                       std::vector<DirectoryEntry>& entries) const override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    std::set<std::string> names;
    bool foundDirectory = false;
    const fs::path relative = relativePath(path);
    const std::vector<fs::path> layers = {
      contentFolder_ / relative,
      (!containsRenpyEngine_ && hasPrefix(path, "/lib"))
        ? baseFolder_ / relative : modFolder_ / relative,
      baseFolder_ / relative
    };

    auto addLayer = [&](const fs::path& directory) -> Status {
      std::error_code error;
      if (!fs::exists(directory, error)) return filesystemStatus(error);
      if (error) return filesystemStatus(error);
      if (!fs::is_directory(directory, error)) return error ? filesystemStatus(error)
                                                            : Status::NotDirectory;
      foundDirectory = true;
      fs::directory_iterator iterator(directory, error), end;
      if (error) return filesystemStatus(error);
      for (; iterator != end; iterator.increment(error)) {
        if (error) return filesystemStatus(error);
        const std::string name = iterator->path().filename().string();
        if (name == ".vclubmgr" ||
            whiteouts_->contains((relative / name).generic_string())) continue;
        names.insert(name);
      }
      return filesystemStatus(error);
    };

    static const fs::path patchesRoot = "/game/_launcher_patches";
    if (hasPrefix(path, patchesRoot.generic_string())) {
      const fs::path suffix = relativePath(path).lexically_relative(
        relativePath(patchesRoot.generic_string()));
      const Status status = addLayer(launcherRoot_ / "assets" / "patches" / suffix);
      if (status != Status::Ok && status != Status::NoSuchEntry) return status;
    } else if (path == "/game") {
      names.insert("_launcher_patches");
      foundDirectory = true;
    }

    for (const auto& layer : layers) {
      const Status status = addLayer(layer);
      if (status != Status::Ok && status != Status::NoSuchEntry) return status;
    }
    if (!foundDirectory) return Status::NoSuchEntry;

    entries.clear();
    entries.push_back({".", {}});
    entries.push_back({"..", {}});
    for (const auto& name : names) {
      DirectoryEntry entry{name, {}};
      const Status status = getMetadata((relative / name).generic_string(), entry.metadata);
      if (status == Status::Ok) entries.push_back(std::move(entry));
      else if (status != Status::NoSuchEntry) return status;
    }
    return Status::Ok;
  }

  Status openFile(const std::string& path, const OpenOptions& options,
                  HandleContext& handle) override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    if (options.write) {
      const Status copyStatus = ensureCopy(path);
      if (copyStatus != Status::Ok &&
          !(options.create && copyStatus == Status::NoSuchEntry)) return copyStatus;
    }
    fs::path resolved = resolve(path, options.write);
    std::error_code error;
    if (options.create) {
      fs::create_directories(resolved.parent_path(), error);
      if (error) return filesystemStatus(error);
    }

    auto stream = std::make_unique<std::fstream>();
    std::ios::openmode mode = std::ios::binary;
    if (options.read) mode |= std::ios::in;
    if (options.write) mode |= std::ios::out;
    if (options.truncate) mode |= std::ios::trunc;
    if (options.append) mode |= std::ios::app;
    stream->open(resolved, mode);
    if (!stream->is_open() && options.create) {
      std::ofstream create(resolved, std::ios::binary | std::ios::app);
      if (!create) return Status::PermissionDenied;
      create.close();
      stream->clear();
      stream->open(resolved, mode);
    }
    if (!stream->is_open()) return Status::NoSuchEntry;

    std::lock_guard lock(handlesMutex_);
    const std::uint64_t id = nextHandle_++;
    handle = {id, resolved.string(), options.write, false};
    handles_.emplace(id, OpenHandle{handle, std::move(stream)});
    return Status::Ok;
  }

  Status createFile(const std::string& path, const OpenOptions& options,
                    HandleContext& handle) override {
    const fs::path resolved = resolve(path, true);
    std::error_code error;
    if (fs::exists(resolved, error) && !whiteouts_->contains(path)) return Status::EntryExists;
    if (error) return filesystemStatus(error);
    Status status = whiteouts_->remove(path);
    if (status != Status::Ok) return status;
    fs::create_directories(resolved.parent_path(), error);
    if (error) return filesystemStatus(error);
    OpenOptions createOptions = options;
    createOptions.create = true;
    createOptions.write = true;
    return openFile(path, createOptions, handle);
  }

  Status createDirectory(const std::string& path) override {
    const fs::path resolved = resolve(path, true);
    std::error_code error;
    if (fs::exists(resolved, error) && !whiteouts_->contains(path)) return Status::EntryExists;
    if (error) return filesystemStatus(error);
    Status status = whiteouts_->remove(path);
    if (status != Status::Ok) return status;
    fs::create_directories(resolved, error);
    return filesystemStatus(error);
  }

  Status read(std::uint64_t handleId, std::uint64_t offset,
              std::span<std::byte> buffer, std::size_t& bytesRead) override {
    std::lock_guard lock(handlesMutex_);
    auto it = handles_.find(handleId);
    if (it == handles_.end()) return Status::InvalidArgument;
    auto& stream = *it->second.stream;
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(offset));
    if (!stream) return Status::IoError;
    stream.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
    bytesRead = static_cast<std::size_t>(stream.gcount());
    return stream.bad() ? Status::IoError : Status::Ok;
  }

  Status write(std::uint64_t handleId, std::uint64_t offset,
               std::span<const std::byte> buffer, std::size_t& bytesWritten) override {
    std::lock_guard lock(handlesMutex_);
    auto it = handles_.find(handleId);
    if (it == handles_.end()) return Status::InvalidArgument;
    auto& stream = *it->second.stream;
    stream.clear();
    stream.seekp(static_cast<std::streamoff>(offset));
    if (!stream) return Status::IoError;
    stream.write(reinterpret_cast<const char*>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));
    if (!stream) return Status::IoError;
    bytesWritten = buffer.size();
    stream.flush();
    return stream ? Status::Ok : Status::IoError;
  }

  Status close(std::uint64_t handleId) override {
    std::lock_guard lock(handlesMutex_);
    auto it = handles_.find(handleId);
    if (it == handles_.end()) return Status::InvalidArgument;
    it->second.stream->close();
    handles_.erase(it);
    return Status::Ok;
  }

  Status setPendingDelete(std::uint64_t handleId, bool pending) override {
    std::lock_guard lock(handlesMutex_);
    auto it = handles_.find(handleId);
    if (it == handles_.end()) return Status::InvalidArgument;
    it->second.context.pendingDelete = pending;
    return Status::Ok;
  }

  Status unlink(const std::string& path) override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    const fs::path relative = relativePath(path);
    const fs::path content = contentFolder_ / relative;
    const fs::path mod = modFolder_ / relative;
    const fs::path base = baseFolder_ / relative;
    std::error_code error;
    const bool contentExists = fs::exists(content, error);
    if (error) return filesystemStatus(error);
    const bool modExists = fs::exists(mod, error) && !fs::is_directory(mod, error);
    if (error) return filesystemStatus(error);
    const bool baseExists = fs::exists(base, error) && !fs::is_directory(base, error);
    if (error) return filesystemStatus(error);
    if (!contentExists && !modExists && !baseExists) return Status::NoSuchEntry;
    if (contentExists) {
      if (fs::is_directory(content, error)) return Status::IsDirectory;
      if (error) return filesystemStatus(error);
      fs::remove(content, error);
      if (error) return filesystemStatus(error);
    }
    if ((modExists || baseExists) && !bypassWhiteout(path)) return whiteouts_->add(path);
    return Status::Ok;
  }

  Status removeDirectory(const std::string& path) override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    std::vector<DirectoryEntry> entries;
    Status status = readDirectory(path, entries);
    if (status != Status::Ok) return status;
    if (entries.size() > 2) return Status::DirectoryNotEmpty;
    const fs::path relative = relativePath(path);
    const fs::path content = contentFolder_ / relative;
    const fs::path mod = modFolder_ / relative;
    const fs::path base = baseFolder_ / relative;
    std::error_code error;
    const bool contentExists = fs::is_directory(content, error);
    if (error) return filesystemStatus(error);
    const bool modExists = fs::is_directory(mod, error);
    if (error) return filesystemStatus(error);
    const bool baseExists = fs::is_directory(base, error);
    if (error) return filesystemStatus(error);
    if (contentExists) {
      fs::remove_all(content, error);
      if (error) return filesystemStatus(error);
    }
    if ((modExists || baseExists) && !bypassWhiteout(path)) return whiteouts_->add(path);
    return Status::Ok;
  }

  Status rename(const std::string& oldPath, const std::string& newPath) override {
    if (whiteouts_->contains(oldPath)) return Status::NoSuchEntry;
    const fs::path source = resolve(oldPath, true);
    const fs::path destination = resolve(newPath, true);
    std::error_code error;
    const bool lowerExists = fs::exists(modFolder_ / relativePath(oldPath), error) ||
                             fs::exists(baseFolder_ / relativePath(oldPath), error);
    if (error) return filesystemStatus(error);
    Status status = ensureCopy(oldPath);
    if (status != Status::Ok) return status;
    fs::create_directories(destination.parent_path(), error);
    if (error) return filesystemStatus(error);
    fs::rename(source, destination, error);
    if (error) return filesystemStatus(error);
    if (lowerExists && !bypassWhiteout(oldPath)) {
      status = whiteouts_->add(oldPath);
      if (status != Status::Ok) return status;
    }
    return whiteouts_->remove(newPath);
  }

  Status setAttributes(const std::string& path, const Metadata& metadata) override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    Status status = ensureCopy(path);
    if (status != Status::Ok) return status;
    const fs::path resolved = resolve(path, true);
    std::error_code error;
    auto permissions = static_cast<fs::perms>(metadata.permissions & 0777U);
    constexpr fs::perms writePermissions = fs::perms::owner_write |
                                           fs::perms::group_write |
                                           fs::perms::others_write;
    if (metadata.isReadOnly) permissions &= ~writePermissions;
    else permissions |= fs::perms::owner_write;
    fs::permissions(resolved, permissions,
                    fs::perm_options::replace, error);
    if (error) return filesystemStatus(error);
    if (metadata.modificationTime != 0) {
      fs::last_write_time(resolved, fromUnixSeconds(metadata.modificationTime), error);
    }
    return filesystemStatus(error);
  }

  Status setFileSize(const std::string& path, std::uint64_t size) override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    Status status = ensureCopy(path);
    if (status != Status::Ok) return status;
    std::error_code error;
    fs::resize_file(resolve(path, true), size, error);
    return filesystemStatus(error);
  }

  Status access(const std::string& path, int mode) const override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    std::error_code error;
    const auto permissions = fs::status(resolve(path), error).permissions();
    if (error) return filesystemStatus(error);
    if (!fs::exists(resolve(path), error)) return error ? filesystemStatus(error)
                                                       : Status::NoSuchEntry;
    if ((mode & 4) && (permissions & (fs::perms::owner_read | fs::perms::group_read |
                                      fs::perms::others_read)) == fs::perms::none)
      return Status::PermissionDenied;
    if ((mode & 2) && (permissions & (fs::perms::owner_write | fs::perms::group_write |
                                      fs::perms::others_write)) == fs::perms::none)
      return Status::PermissionDenied;
    if ((mode & 1) && (permissions & (fs::perms::owner_exec | fs::perms::group_exec |
                                      fs::perms::others_exec)) == fs::perms::none)
      return Status::PermissionDenied;
    return Status::Ok;
  }

  Status getExtendedAttribute(const std::string& path, const std::string& name,
                              char* value, std::size_t size,
                              std::size_t& valueSize) const override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    if (name != "vcmgr.path") return Status::NoData;
    const std::string resolved = resolve(path).string();
    valueSize = resolved.size();
    if (size == 0) return Status::Ok;
    if (size < valueSize) return Status::Range;
    std::memcpy(value, resolved.data(), valueSize);
    return Status::Ok;
  }

  Status readLink(const std::string& path, std::string& target) const override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    std::error_code error;
    target = fs::read_symlink(resolve(path), error).string();
    return filesystemStatus(error);
  }

  Status statfs(const std::string& path, StatFs& stats) const override {
    if (whiteouts_->contains(path)) return Status::NoSuchEntry;
    std::error_code error;
    const auto space = fs::space(resolve(path), error);
    if (error) return filesystemStatus(error);
    stats.blockSize = stats.fragmentSize = 4096;
    stats.blocks = space.capacity / stats.blockSize;
    stats.freeBlocks = space.free / stats.blockSize;
    stats.availableBlocks = space.available / stats.blockSize;
    return Status::Ok;
  }

private:
  static fs::path relativePath(const std::string& path) {
    fs::path relative = fs::path(path).relative_path().lexically_normal();
    while (!relative.empty() && relative.filename().empty()) relative = relative.parent_path();
    return relative == "." ? fs::path{} : relative;
  }

  static bool hasPrefix(const std::string& path, const std::string& prefix) {
    const fs::path pathParts = relativePath(path);
    const fs::path prefixParts = relativePath(prefix);
    auto pathPart = pathParts.begin();
    for (auto prefixPart = prefixParts.begin(); prefixPart != prefixParts.end(); ++prefixPart) {
      if (pathPart == pathParts.end()) return false;
#ifdef _WIN32
      const std::string pathComponent = pathPart->string();
      const std::string prefixComponent = prefixPart->string();
      if (pathComponent.size() != prefixComponent.size()) return false;
      for (std::size_t i = 0; i < pathComponent.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(pathComponent[i])) !=
            std::tolower(static_cast<unsigned char>(prefixComponent[i])))
          return false;
      }
#else
      if (*pathPart != *prefixPart) return false;
#endif
      ++pathPart;
    }
    return true;
  }

  bool bypassWhiteout(const std::string& path) const { return hasPrefix(path, "/lib"); }

  fs::path resolve(const std::string& path, bool write = false) const {
    auto resolved = resolver_->getPath(path, write);
    
    return resolved;
  }

  Status ensureCopy(const std::string& path) const {
    const fs::path destination = contentFolder_ / relativePath(path);
    std::error_code error;
    if (fs::exists(destination, error)) {
      if (error) return filesystemStatus(error);
      if (!fs::is_directory(destination, error)) return filesystemStatus(error);
      return error ? filesystemStatus(error) : copyDirectory(path, destination);
    }
    if (error) return filesystemStatus(error);
    const fs::path source = resolve(path);
    if (!fs::exists(source, error)) return error ? filesystemStatus(error)
                                                 : Status::NoSuchEntry;
    fs::create_directories(destination.parent_path(), error);
    if (error) return filesystemStatus(error);
    if (fs::is_directory(source, error)) {
      fs::create_directories(destination, error);
      if (error) return filesystemStatus(error);
      return copyDirectory(path, destination);
    } else {
      fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
    }
    return filesystemStatus(error);
  }

  Status copyDirectory(const std::string& path, const fs::path& destination) const {
    std::vector<DirectoryEntry> entries;
    Status status = readDirectory(path, entries);
    if (status != Status::Ok) return status;
    std::error_code error;
    for (const auto& entry : entries) {
      if (entry.name == "." || entry.name == "..") continue;
      const std::string child = (fs::path(path) / entry.name).generic_string();
      const fs::path childDestination = destination / entry.name;
      if (entry.metadata.isDirectory) {
        fs::create_directories(childDestination, error);
        if (error) return filesystemStatus(error);
        status = copyDirectory(child, childDestination);
        if (status != Status::Ok) return status;
      } else {
        fs::copy_file(resolve(child), childDestination,
                      fs::copy_options::overwrite_existing, error);
        if (error) return filesystemStatus(error);
      }
    }
    return Status::Ok;
  }

  fs::path launcherRoot_;
  fs::path baseFolder_;
  fs::path modFolder_;
  fs::path deltaFolder_;
  fs::path contentFolder_;
  bool containsRenpyEngine_ = false;
  std::unique_ptr<WhiteoutStore> whiteouts_;
  std::unique_ptr<VFSPathResolver> resolver_;
  struct OpenHandle {
    HandleContext context;
    std::unique_ptr<std::fstream> stream;
  };
  std::mutex handlesMutex_;
  std::map<std::uint64_t, OpenHandle> handles_;
  std::uint64_t nextHandle_ = 1;
};

} // namespace

std::unique_ptr<VFSCoreOperations> createCore(std::string launcherRoot,
                                               std::string modId,
                                               std::string modPath) {
  return std::make_unique<FilesystemCore>(std::move(launcherRoot), std::move(modId),
                                          std::move(modPath));
}

} // namespace vfs
