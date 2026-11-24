#include "utils/macros.h"
#include <system_error>
#ifdef MVC_VFS_AVAILABLE
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <mutex>
#include <ostream>
#include "utils/ProfileSettings.hpp"
#include <shared_mutex>
#include <functional>
#include <string>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <memory>
#endif
#include "vfs.hpp"
#include "VFSPathResolver.hpp"
#include "utils/ModIndex.hpp"
#include "consts.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>

#include <nlohmann/json.hpp>

#ifdef MVC_VFS_AVAILABLE

// inline platform-specific macros to make the code look more readable
#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>

typedef unsigned short mode_t;
typedef unsigned long pid_t;
typedef SSIZE_T ssize_t;

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct statvfs {
    unsigned long f_bsize, f_frsize;
    uint64_t f_blocks, f_bfree, f_bavail;
    unsigned long f_files, f_ffree, f_favail, f_fsid, f_flag, f_namemax;
};

inline int lstat(const char *path, struct stat *stbuf) { return ::stat(path, stbuf); }
inline int open_file(const char *path, int flags, int mode = 0) { return ::_open(path, flags, mode); }
inline int close_file(int fd) { return ::_close(fd); }
inline int access_file(const char *path, int mode) { return ::_access(path, mode); }
inline int chmod_file(const char *path, mode_t mode) { return ::_chmod(path, mode); }

static inline ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    _lseeki64(fd, offset, SEEK_SET);
    return _read(fd, buf, static_cast<unsigned int>(count));
}
static inline ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    _lseeki64(fd, offset, SEEK_SET);
    return _write(fd, buf, static_cast<unsigned int>(count));
}
#else
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
extern char** environ;

#ifndef O_BINARY
#define O_BINARY 0
#endif
#define fuse_off_t off_t

inline int open_file(const char *path, int flags, int mode = 0) { return ::open(path, flags, mode); }
inline int close_file(int fd) { return ::close(fd); }
inline int access_file(const char *path, int mode) { return ::access(path, mode); }
inline int chmod_file(const char *path, mode_t mode) { return ::chmod(path, mode); }
#endif

namespace fs = std::filesystem;

static int fuse_error(const std::error_code &ec) {
    if (!ec) return 0;
    if (ec == std::errc::no_such_file_or_directory) return -ENOENT;
    if (ec == std::errc::file_exists) return -EEXIST;
    if (ec == std::errc::not_a_directory) return -ENOTDIR;
    if (ec == std::errc::is_a_directory) return -EISDIR;
    if (ec == std::errc::permission_denied) return -EACCES;
    if (ec == std::errc::directory_not_empty) return -ENOTEMPTY;
    if (ec == std::errc::operation_not_permitted) return -EPERM;
    if (ec == std::errc::filename_too_long) return -ENAMETOOLONG;
    if (ec == std::errc::no_space_on_device) return -ENOSPC;
    if (ec == std::errc::read_only_file_system) return -EROFS;
    if (ec == std::errc::invalid_argument) return -EINVAL;
    return -EIO;
}

fs::path get_appdata_dir() {
    return fs::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString());
}

constexpr size_t constexpr_strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}


static constexpr auto g_launcherPatchesPath = "/game/_launcher_patches";
static constexpr auto g_lppLeadingSlash = "/game/_launcher_patches/";
static constexpr size_t g_lppSize = constexpr_strlen(g_launcherPatchesPath);

// ============================================================================
// 2. LibbiVFS
// ============================================================================
void* fuse_init(struct fuse_conn_info *, struct fuse_config *cfg);

struct VFSStartupConfigs {
    const ModsIndex::Mod mod; 

    const std::string mountpoint;
};

/*
 * Manages a virtual folder that is used by Ren'Py for launching the game.
 *
 * --- nerdy corner i think ---
 * 
 * This is a delta VFS that stacks (at least) 3 different layer of folders.
 * Specifically:
 * Delta Layer (folder that store new writes)
 *      ||
 *      v
 *  Mod Layer (the mod's folder) (Read-Only)
 *      ||
 *      v
 * Base Layer (base game's folder) (Read-Only)
 *
 * the 4th "layer" is the launcher's .rpy hooks inserted into game/_launcher_patches/
 */
class LibbiVFS {
private:
    fs::path baseFolder;
    fs::path modFolder;
    VFSStartupConfigs startupConfigs;
    bool containsRenpyEngine;

    nlohmann::json modsJson;
    nlohmann::json settingsJson;

    friend void* fuse_init(struct fuse_conn_info *, struct fuse_config *cfg);

    fs::path dbFolder;
    fs::path deltaFolder;
    fs::path contentFolder;
    fs::path whiteoutFilePath;
    std::unordered_set<std::string> whiteouts;
    mutable std::shared_mutex dbMutex;
    std::unique_ptr<VFSPathResolver> pathResolver;

    std::string strip_leading_slash(const std::string &path) const {
        return (!path.empty() && (path[0] == '/' || path[0] == '\\')) ? path.substr(1) : path;
    }

    std::string normalize_path(const std::string &path) const {
        std::string p = path;
        while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
        if (p.empty()) return "/";
        return (p[0] != '/' && p[0] != '\\') ? "/" + p : p;
    }

    void load_whiteouts() {
        std::unique_lock lock(dbMutex);
        whiteouts.clear();
        std::error_code ec;
        if (!fs::exists(whiteoutFilePath, ec)) {
            if (ec) throw std::system_error(ec, "Failed to check whiteout file");
            return;
        }

        std::ifstream f(whiteoutFilePath);
        if (!f) throw std::system_error(std::make_error_code(std::errc::io_error), "Failed to open whiteout file");
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) whiteouts.insert(normalize_path(line));
        }
    }

    int save_whiteouts() {
        std::unique_lock lock(dbMutex);
        std::error_code ec;
        fs::create_directories(whiteoutFilePath.parent_path(), ec);
        if (ec) return fuse_error(ec);
        std::ofstream f(whiteoutFilePath, std::ios::trunc);
        if (!f) return -EIO;
        for (const auto &w : whiteouts) f << w << "\n";
        return f ? 0 : -EIO;
    }

    bool is_whiteouted(const std::string &path) const {
        std::shared_lock lock(dbMutex);
        std::string norm = normalize_path(path);
        if (whiteouts.count(norm)) return true;
        
        std::string parent = norm;
        while (true) {
            size_t idx = parent.find_last_of("/\\");
            if (idx == std::string::npos || idx == 0) break;
            parent = parent.substr(0, idx);
            if (whiteouts.count(parent)) return true;
        }
        return false;
    }

    int add_whiteout(const std::string &path) {
        {
            std::unique_lock lock(dbMutex);
            whiteouts.insert(normalize_path(path));
        }
        return save_whiteouts();
    }

    int remove_whiteout(const std::string &path) {
        {
            std::unique_lock lock(dbMutex);
            whiteouts.erase(normalize_path(path));
        }
        return save_whiteouts();
    }

    fs::path launcherRoot;
    bool verboseLog = false;

public:
    LibbiVFS(std::string launcherRoot, decltype(startupConfigs) startupConfigs)
        : launcherRoot(launcherRoot), startupConfigs(startupConfigs) {

        auto settings = ProfileSettings::get();
        baseFolder = settings->value(STK_BASEPATH).toString().toStdString();
        if (baseFolder.empty()) {
            std::cerr << "Base game installation path not found in settings.json. Please set it in the launcher." << std::endl;
            throw std::runtime_error("Base game installation path not found.");
        }

        modFolder = startupConfigs.mod.modPath;

        std::error_code ec;
        containsRenpyEngine = fs::exists(modFolder / "lib", ec);
        if (ec) throw std::system_error(ec, "Failed to check mod engine directory");
        dbFolder = fs::path(QSettings().value(GSTK_STORAGE_PATH).toString().toStdString()) / "session_data";
        deltaFolder = dbFolder / "delta" / startupConfigs.mod.id;
        contentFolder = deltaFolder / "content";
        whiteoutFilePath = deltaFolder / ".whiteouts";
        fs::create_directories(contentFolder, ec);
        if (ec) throw std::system_error(ec, "Failed to create mod delta directory");
        load_whiteouts();
        pathResolver = std::make_unique<VFSPathResolver>(
            baseFolder,
            modFolder,
            deltaFolder,
            launcherRoot,
            startupConfigs.mod.id,
            containsRenpyEngine);
    }

    void enableYapping() { verboseLog = true; }

    std::string get_path_2(const std::string &path, bool write = false) const {
        return pathResolver->getPath(path, write);
    }

    std::string get_path(const std::string &path, bool write = false) const {
        auto ret = get_path_2(path, write);
        //qDebug() << "Requested" << path << "| Resolved to" << ret;
        return ret;
    }

    int ensure_content_copy(const std::string &path) {
        fs::path content_path = contentFolder / strip_leading_slash(normalize_path(path));
        std::error_code ec;
        if (fs::exists(content_path, ec)) return 0;
        if (ec) return fuse_error(ec);

        std::string source_path = get_path(path);
        if (!fs::exists(source_path, ec)) return ec ? fuse_error(ec) : -ENOENT;

        fs::create_directories(content_path.parent_path(), ec);
        if (ec) return fuse_error(ec);
        const bool source_is_directory = fs::is_directory(source_path, ec);
        if (ec) return fuse_error(ec);
        if (source_is_directory) {
            fs::create_directories(content_path, ec);
        } else {
            fs::copy(source_path, content_path, fs::copy_options::overwrite_existing, ec);
        }
        return fuse_error(ec);
    }

    bool should_bypass_whiteout(const std::string &path) const {
        return (path.rfind("/lib/", 0) == 0 || path == "/lib" ||
                path.rfind("\\lib\\", 0) == 0 || path == "\\lib");
    }

    // one thing is to send the file's xattr if any (maybe?), plus "vcmgr.path" setting to the file's resolved path
    int getxattr(const std::string &path, const std::string &name, char *value, size_t size) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path);
        
        const char* val = NULL;
        if (name == "vcmgr.path") {
            val = full_path.c_str();
        } else {
            return -ENODATA;
        }

        size_t val_len = std::strlen(val);
        if (size == 0) return static_cast<int>(val_len);
        if (size < val_len) return -ERANGE;

        std::memcpy(value, val, val_len);
        return static_cast<int>(val_len);
    }

    int getattr(const std::string &path, struct stat *stbuf) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path);
        return (lstat(full_path.c_str(), stbuf) == -1) ? -errno : 0;
    }

    int readdir(const std::string &path, const std::function<void(const std::string&)> &filler) {
        if (is_whiteouted(path)) return -ENOENT;

        std::unordered_set<std::string> dirents = {".", ".."};
        std::string stripped = strip_leading_slash(normalize_path(path));
        fs::path content_path = (contentFolder / stripped);
        fs::path base_path = (baseFolder / stripped);
        fs::path mod_path = (!containsRenpyEngine && (path.rfind("/lib", 0) == 0))
                             ? base_path : (modFolder / stripped);
        bool found = false;

        auto populate = [&](const fs::path &dir) {
            std::error_code ec;
            if (!fs::is_directory(dir, ec)) return ec ? fuse_error(ec) : 0;
            found = true;
            fs::directory_iterator it(dir, ec), end;
            if (ec) return fuse_error(ec);
            while (it != end) {
                auto name = it->path().filename();
                if (name.string() != ".vclubmgr" && !is_whiteouted((path / name).string())) {
                    dirents.insert(name.string());
                }
                it.increment(ec);
                if (ec) return fuse_error(ec);
            }
            return 0;
        };

        if (path.rfind(g_launcherPatchesPath, 0) == 0) {
            fs::path patchesSubdir = (launcherRoot / "assets" / "patches") / path.substr((path.rfind(g_lppLeadingSlash, 0) == 0) ? g_lppSize+1 : g_lppSize);
            int res = populate(patchesSubdir);
            if (res) return res;
        }
        else if (path == "/game") {
          dirents.insert("_launcher_patches");
        }

        for (const auto &dir : {content_path, mod_path, base_path}) {
            int res = populate(dir);
            if (res) return res;
        }

        if (!found) return -ENOENT;
        for (const auto &entry : dirents) filler(entry);
        //qDebug() << dirents;
        return 0;
    }

    int open(const std::string &path, int flags, uint64_t &fh) {
        if (is_whiteouted(path)) return -ENOENT;
        const bool writing = (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) != 0;
        if (writing) {
            int res = ensure_content_copy(path);
            if (res && (!(flags & O_CREAT) || res != -ENOENT)) return res;
        }
        std::string full_path = get_path(path, writing);
        int fd = open_file(full_path.c_str(), flags | O_BINARY);
        if (fd == -1) return -errno;
        fh = static_cast<uint64_t>(fd);
        return 0;
    }

    int read(uint64_t fh, char *buf, size_t size, off_t offset) {
        int res = static_cast<int>(pread(static_cast<int>(fh), buf, size, offset));
        return (res == -1) ? -errno : res;
    }

    int write(uint64_t fh, const char *buf, size_t size, off_t offset) {
        int res = static_cast<int>(pwrite(static_cast<int>(fh), buf, size, offset));
        return (res == -1) ? -errno : res;
    }

    int release(uint64_t fh) {
        close_file(static_cast<int>(fh));
        return 0;
    }

    int readlink(const std::string &path, char *buf, size_t size) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path);
#ifdef Q_OS_WIN
        std::error_code ec;
        std::string target = fs::read_symlink(full_path, ec).string();
        if (ec) return fuse_error(ec);
        size_t copy_len = (std::min)(size - 1, target.size());
        std::memcpy(buf, target.c_str(), copy_len);
        buf[copy_len] = '\0';
        return 0;
#else
        ssize_t res = ::readlink(full_path.c_str(), buf, size - 1);
        if (res == -1) return -errno;
        buf[res] = '\0';
        return 0;
#endif
    }

    int access(const std::string &path, int mask) {
        if (is_whiteouted(path)) return -ENOENT;
        return (access_file(get_path(path).c_str(), mask) == -1) ? -errno : 0;
    }

    int statfs(const std::string &path, struct statvfs *stbuf) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path);
#ifdef Q_OS_WIN
        ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceExA(full_path.c_str(), &freeBytes, &totalBytes, &totalFreeBytes)) {
            stbuf->f_bsize = stbuf->f_frsize = 4096;
            stbuf->f_blocks = totalBytes.QuadPart / 4096;
            stbuf->f_bfree = totalFreeBytes.QuadPart / 4096;
            stbuf->f_bavail = freeBytes.QuadPart / 4096;
            return 0;
        }
        return -EIO;
#else
        return (::statvfs(full_path.c_str(), stbuf) == -1) ? -errno : 0;
#endif
    }

    int create(const std::string &path, mode_t mode) {
        std::string full_path = get_path(path, true);
        std::error_code ec;
        if (fs::exists(full_path, ec) && !is_whiteouted(path)) return -EEXIST;
        if (ec) return fuse_error(ec);

        int res = remove_whiteout(path);
        if (res) return res;
        fs::create_directories(fs::path(full_path).parent_path(), ec);
        if (ec) return fuse_error(ec);
        int fd = open_file(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
        if (fd == -1) return -errno;
        close_file(fd);
        return 0;
    }

    int truncate(const std::string &path, off_t size) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path, true);
        std::error_code ec;
        if (!fs::exists(full_path, ec)) {
            if (ec) return fuse_error(ec);
            std::string base_path = get_path(path, false);
            if (!fs::exists(base_path, ec)) return ec ? fuse_error(ec) : -ENOENT;
            fs::create_directories(fs::path(full_path).parent_path(), ec);
            if (ec) return fuse_error(ec);
            fs::copy_file(base_path, full_path, fs::copy_options::overwrite_existing, ec);
            if (ec) return fuse_error(ec);
        }
#ifdef Q_OS_WIN
        int fd = open_file(full_path.c_str(), O_RDWR | O_BINARY);
        if (fd == -1) return -errno;
        int res = _chsize(fd, static_cast<long>(size));
        close_file(fd);
        return (res != 0) ? -errno : 0;
#else
        return (::truncate(full_path.c_str(), size) == -1) ? -errno : 0;
#endif
    }

    int mkdir(const std::string &path, mode_t mode) {
        std::string full_path = get_path(path, true);
        std::error_code ec;
        if (fs::exists(full_path, ec) && !is_whiteouted(path)) return -EEXIST;
        if (ec) return fuse_error(ec);

        int res = remove_whiteout(path);
        if (res) return res;
        fs::create_directories(full_path, ec);
        if (ec) return fuse_error(ec);
        if (chmod_file(full_path.c_str(), mode) == -1) return -errno;
        return 0;
    }

    int rmdir(const std::string &path) {
      if (is_whiteouted(path)) return -ENOENT;

      const std::string stripped = strip_leading_slash(normalize_path(path));
      fs::path content_path = contentFolder / stripped;
      fs::path mod_path = modFolder / stripped;
      fs::path base_path = baseFolder / stripped;

      std::error_code ec;
      auto is_directory = [&ec](const fs::path& candidate, bool &result) {
          result = false;
          if (!fs::exists(candidate, ec)) return ec ? fuse_error(ec) : 0;
          result = fs::is_directory(candidate, ec);
          return ec ? fuse_error(ec) : 0;
      };
      bool exists_in_content, exists_in_mod, exists_in_base;
      int res = is_directory(content_path, exists_in_content);
      if (res) return res;
      res = is_directory(mod_path, exists_in_mod);
      if (res) return res;
      res = is_directory(base_path, exists_in_base);
      if (res) return res;

      if (!exists_in_content && !exists_in_mod && !exists_in_base) {
          return -ENOENT;
      }

      if (exists_in_content) {
          fs::remove_all(content_path, ec);
          if (ec) return fuse_error(ec);
          if (fs::exists(content_path, ec)) return ec ? fuse_error(ec) : -EIO;
          if (ec) return fuse_error(ec);
      }

      if ((exists_in_mod || exists_in_base) && !should_bypass_whiteout(path)) {
          res = add_whiteout(path);
          if (res) return res;
      }

      return 0;
    }

    int unlink(const std::string &path) {
      if (is_whiteouted(path)) return -ENOENT;

      const std::string stripped = strip_leading_slash(normalize_path(path));
      fs::path content_path = contentFolder / stripped;
      fs::path mod_path = modFolder / stripped;
      fs::path base_path = baseFolder / stripped;

      std::error_code ec;
      auto is_file = [&ec](const fs::path& candidate, bool &result) {
          result = false;
          if (!fs::exists(candidate, ec)) return ec ? fuse_error(ec) : 0;
          result = !fs::is_directory(candidate, ec);
          return ec ? fuse_error(ec) : 0;
      };
      bool exists_in_content, exists_in_mod, exists_in_base;
      int res = is_file(content_path, exists_in_content);
      if (res) return res;
      res = is_file(mod_path, exists_in_mod);
      if (res) return res;
      res = is_file(base_path, exists_in_base);
      if (res) return res;

      if (!exists_in_content && !exists_in_mod && !exists_in_base) {
          return -ENOENT;
      }

      if (exists_in_content) {
          const bool removed = fs::remove(content_path, ec);
          if (ec) return fuse_error(ec);
          if (!removed && fs::exists(content_path, ec)) return ec ? fuse_error(ec) : -EIO;
          if (ec) return fuse_error(ec);
      }

      if ((exists_in_mod || exists_in_base) && !should_bypass_whiteout(path)) {
          res = add_whiteout(path);
          if (res) return res;
      }

      return 0;
    }

    int rename(const std::string &oldpath, const std::string &newpath) {
        if (is_whiteouted(oldpath)) return -ENOENT;

        std::string old_full = get_path(oldpath, true);
        std::string new_full = get_path(newpath, true);

        std::error_code ec;
        bool exists_in_mod = fs::exists(modFolder / strip_leading_slash(oldpath), ec);
        if (ec) return fuse_error(ec);
        bool exists_in_base = fs::exists(baseFolder / strip_leading_slash(oldpath), ec);
        if (ec) return fuse_error(ec);

        if (!fs::exists(old_full, ec)) {
            if (ec) return fuse_error(ec);
            std::string old_base = get_path(oldpath, false);
            if (!fs::exists(old_base, ec)) return ec ? fuse_error(ec) : -ENOENT;
            fs::create_directories(fs::path(old_full).parent_path(), ec);
            if (ec) return fuse_error(ec);
            bool old_is_directory = fs::is_directory(old_base, ec);
            if (ec) return fuse_error(ec);
            if (old_is_directory) {
                fs::copy(old_base, old_full, fs::copy_options::recursive, ec);
            } else {
                fs::copy_file(old_base, old_full, ec);
            }
            if (ec) return fuse_error(ec);
        }

        fs::create_directories(fs::path(new_full).parent_path(), ec);
        if (ec) return fuse_error(ec);
        if (::rename(old_full.c_str(), new_full.c_str()) == -1) return -errno;

        if (exists_in_mod || exists_in_base) {
            int res = add_whiteout(oldpath);
            if (res) return res;
        }
        return remove_whiteout(newpath);
    }

    int chmod(const std::string &path, mode_t mode) {
        if (is_whiteouted(path)) return -ENOENT;
        int res = ensure_content_copy(path);
        if (res) return res;
        std::string full_path = get_path(path, true);
        return (chmod_file(full_path.c_str(), mode) == -1) ? -errno : 0;
    }

    int utimens(const std::string &path, const struct timespec tv[2]) {
        if (is_whiteouted(path)) return -ENOENT;
        int res = ensure_content_copy(path);
        if (res) return res;
        std::string full_path = get_path(path, true);
#ifdef Q_OS_WIN 
        std::error_code ec;
        auto file_time = std::chrono::system_clock::from_time_t(tv[1].tv_sec);
        fs::last_write_time(full_path, std::chrono::time_point_cast<fs::file_time_type::duration>(
            file_time - std::chrono::system_clock::now() + fs::file_time_type::clock::now()), ec);
        return fuse_error(ec);
#else
        return (utimensat(AT_FDCWD, full_path.c_str(), tv, 0) == -1) ? -errno : 0;
#endif
    }
};

// ============================================================================
// 3. Unified FUSE Glue Logic
// ============================================================================

namespace FUSE_Glue {
    static LibbiVFS* get_fs() {
        auto* ctx = fuse_get_context();
        return (ctx && ctx->private_data) ? static_cast<LibbiVFS*>(ctx->private_data) : nullptr;
    }

#ifdef Q_OS_WIN
    using stat_t = fuse_stat;
    using statvfs_t = fuse_statvfs;
    using mode_type = fuse_mode_t;
    using timespec_t = fuse_timespec;
#else
    using stat_t = struct stat;
    using statvfs_t = struct statvfs;
    using mode_type = mode_t;
    using timespec_t = struct timespec;
#endif

    static int getattr_glue(const char *path, stat_t *stbuf, struct fuse_file_info *fi) {
        struct stat std_st = {};
        int res = get_fs()->getattr(path, &std_st);
        if (res == 0) {
            std::memset(stbuf, 0, sizeof(*stbuf));
            stbuf->st_mode = std_st.st_mode;
            stbuf->st_nlink = std_st.st_nlink;
            stbuf->st_size = std_st.st_size;
            stbuf->st_uid = std_st.st_uid;
            stbuf->st_gid = std_st.st_gid;
#ifdef Q_OS_WIN 
            // this thing lost execute permission for some reason
            const auto file_type = stbuf->st_mode & _S_IFMT;
            if (file_type == _S_IFREG) {
                stbuf->st_mode |= 0555;
            } else if (file_type == _S_IFDIR) {
                stbuf->st_mode |= 0755;
            }
            stbuf->st_atim = {std_st.st_atime};
            stbuf->st_mtim = {std_st.st_mtime};
            stbuf->st_ctim = {std_st.st_ctime};
#else
            stbuf->st_atim = std_st.st_atim;
            stbuf->st_mtim = std_st.st_mtim;
            stbuf->st_ctim = std_st.st_ctim;
#endif
        }
        return res;
    }

    static int readdir_glue(const char *path, void *buf, fuse_fill_dir_t filler,
                            fuse_off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
        auto cb = [&](const std::string &name) {
            filler(buf, name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        };
        return get_fs()->readdir(path, cb);
    }

    static int open_glue(const char *path, struct fuse_file_info *fi) {
        uint64_t fh = 0;
        int res = get_fs()->open(path, fi->flags, fh);
        if (res == 0) fi->fh = fh;
        return res;
    }

    static int read_glue(const char *path, char *buf, size_t size, fuse_off_t offset, struct fuse_file_info *fi) {
        return get_fs()->read(fi->fh, buf, size, static_cast<off_t>(offset));
    }

    static int write_glue(const char *path, const char *buf, size_t size, fuse_off_t offset, struct fuse_file_info *fi) {
        return get_fs()->write(fi->fh, buf, size, static_cast<off_t>(offset));
    }

    static int release_glue(const char *path, struct fuse_file_info *fi) {
        return get_fs()->release(fi->fh);
    }

    static int readlink_glue(const char *path, char *buf, size_t size) {
        return get_fs()->readlink(path, buf, size);
    }

    static int access_glue(const char *path, int mask) {
        return get_fs()->access(path, mask);
    }

    static int statfs_glue(const char *path, statvfs_t *stbuf) {
        struct statvfs std_st = {};
        int res = get_fs()->statfs(path, &std_st);
        if (res == 0) {
            std::memset(stbuf, 0, sizeof(*stbuf));
            stbuf->f_bsize = std_st.f_bsize;
            stbuf->f_frsize = std_st.f_frsize;
            stbuf->f_blocks = std_st.f_blocks;
            stbuf->f_bfree = std_st.f_bfree;
            stbuf->f_bavail = std_st.f_bavail;
        }
        return res;
    }

    static int create_glue(const char *path, mode_type mode, struct fuse_file_info *fi) {
        int res = get_fs()->create(path, static_cast<mode_t>(mode));
        if (res == 0) return open_glue(path, fi);
        return res;
    }

    static int truncate_glue(const char *path, fuse_off_t size, struct fuse_file_info *fi) {
        return get_fs()->truncate(path, static_cast<off_t>(size));
    }

    static int mkdir_glue(const char *path, mode_type mode) { return get_fs()->mkdir(path, static_cast<mode_t>(mode)); }
    static int rmdir_glue(const char *path) { return get_fs()->rmdir(path); }
    static int unlink_glue(const char *path) { return get_fs()->unlink(path); }
    static int rename_glue(const char *oldpath, const char *newpath, unsigned int flags) { return get_fs()->rename(oldpath, newpath); }
    static int chmod_glue(const char *path, mode_type mode, struct fuse_file_info *fi) { return get_fs()->chmod(path, static_cast<mode_t>(mode)); }

    static int utimens_glue(const char *path, const timespec_t tv[2], struct fuse_file_info *fi) {
        if (!tv) return get_fs()->utimens(path, nullptr);
        struct timespec std_tv[2] = {
            {static_cast<time_t>(tv[0].tv_sec), static_cast<long>(tv[0].tv_nsec)},
            {static_cast<time_t>(tv[1].tv_sec), static_cast<long>(tv[1].tv_nsec)}
        };
        return get_fs()->utimens(path, std_tv);
    }

    static int getxattr_glue(const char *path, const char *name, char *value, size_t size) {
        return get_fs()->getxattr(path, name, value, size);
    }

    static struct fuse_operations get_ops() {
        struct fuse_operations ops = {};
        ops.getattr  = getattr_glue;
        ops.readdir  = readdir_glue;
        ops.open     = open_glue;
        ops.read     = read_glue;
        ops.write    = write_glue;
        ops.release  = release_glue;
        ops.readlink = readlink_glue;
        ops.access   = access_glue;
        ops.statfs   = statfs_glue;
        ops.create   = create_glue;
        ops.truncate = truncate_glue;
        ops.mkdir    = mkdir_glue;
        ops.rmdir    = rmdir_glue;
        ops.unlink   = unlink_glue;
        ops.rename   = rename_glue;
        ops.chmod    = chmod_glue;
        ops.utimens  = utimens_glue;
        ops.getxattr = getxattr_glue;
        ops.init     = fuse_init;
        return ops;
    }
}

// ============================================================================

void* fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    auto publicData = static_cast<LibbiVFS*>(fuse_get_context()->private_data);  
    return publicData;
}

class VFSInstance::Private {
public:
    Private(const QString& launcherRoot, const ModsIndex::Mod& mod)
        : mountpoint(createMountPoint(QString::fromStdString(mod.id))),
          vfs(std::make_unique<LibbiVFS>(
              launcherRoot.toStdString(),
              VFSStartupConfigs{
                  .mod = mod,
                  .mountpoint = mountpoint.toStdString()
              })) {}

    ~Private() {
        unmount();
    }

    bool mount() {
        std::lock_guard lock(mutex);
        if (used || fh || loopThread.joinable()) return false;
        used = true;

        struct fuse_operations ops = FUSE_Glue::get_ops();
        struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);
        fuse_opt_add_arg(&args, "vclubmgr_vfs");
        fuse_opt_add_arg(&args, "-o");
        fuse_opt_add_arg(&args, MVC_WIN("uid=-1") "kernel_cache,attr_timeout=10,entry_timeout=10,negative_timeout=2");

        fh = fuse_new(&args, &ops, sizeof(ops), vfs.get());
        fuse_opt_free_args(&args);
        if (!fh) return false;

        if (fuse_set_signal_handlers(fuse_get_session(fh)) != 0) {
            qCritical() << "Failed to install FUSE signal handlers\n";
        }

        if (fuse_mount(fh, mountpoint.toStdString().c_str()) != 0) {
            qCritical() << "Failed to mount FUSE filesystem at" << mountpoint;
            fuse_remove_signal_handlers(fuse_get_session(fh));
            fuse_destroy(fh);
            fh = nullptr;
            return false;
        }

        loopThread = std::thread([this] {
            fuse_loop_mt(fh, 0);
        });
        return true;
    }

    void unmount() {
        std::unique_lock lock(mutex);
        if (!fh && !loopThread.joinable()) return;

        auto* session = fh ? fuse_get_session(fh) : nullptr;
        auto loop = std::move(loopThread);
        auto handle = fh;
        fh = nullptr;
        lock.unlock();

        // apparently have to unmount fuse before locking
        //
        // fyi previous setup works on arch but stalls on the gh actions build
        if (session) {
            fuse_exit(handle);
            fuse_unmount(handle);
        }

        if (loop.joinable()) loop.join();

        lock.lock();
        if (handle) {
            fuse_remove_signal_handlers(session);
            fuse_destroy(handle);
        }
        lock.unlock();

        if (!mountpoint.isEmpty()) {
            std::error_code ec;
            fs::remove_all(mountpoint.toStdString(), ec);
            if (ec) {
                std::cerr << "Failed to remove mount point: " << ec.message() << '\n';
            }
        }
    }

    QString mountPath() const {
        return mountpoint;
    }

private:
    // Create a temporary mount directory.
    //
    // On Windows, this does NOT actually create the directory or make it exist, 
    // but will try to make sure that the parent one does
    // (because winfsp)
    static QString createMountPoint(const QString& modId) {
      auto path = QDir::tempPath() + "/vclubmgr_mount_" + modId + "_XXXXXX";
#ifndef Q_OS_WIN
        QTemporaryDir directory(path);
        if (!directory.isValid()) {
            throw std::runtime_error("Failed to create temporary mount point");
        }
        directory.setAutoRemove(false);
#else 
      path = QDir::toNativeSeparators(path);
      // just to be safe
      if (QFileInfo::exists(path)) {
          QDir().rmdir(path);
      }
#endif
        return path;

    }

    const QString mountpoint;
    std::unique_ptr<LibbiVFS> vfs;
    fuse* fh = nullptr;
    std::thread loopThread;
    bool used = false;
    mutable std::mutex mutex;
};

VFSInstance::VFSInstance(const QString& launcherRoot, const ModsIndex::Mod& mod)
    : d(std::make_unique<Private>(launcherRoot, mod)) {}

VFSInstance::~VFSInstance() = default;

bool VFSInstance::mount() {
    return d->mount();
}

void VFSInstance::unmount() {
    d->unmount();
}

QString VFSInstance::mountPath() const {
    return d->mountPath();
}
#else
// VFSInstance is noop
VFSInstance::VFSInstance(const QString&, const ModsIndex::Mod&) {
    throw std::runtime_error("VFS is not available on this platform.");
}
bool VFSInstance::mount() {
    return false;
}
void VFSInstance::unmount() {
}
QString VFSInstance::mountPath() const {
    return QString();
}
#endif
