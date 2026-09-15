#include <mutex>
#include <ostream>
#include "vfs.hpp"
#include "utils/ModIndex.hpp"
#include "utils/ProfileSettings.hpp"
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <shared_mutex>
#include <functional>
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cerrno>
#include <thread>
#include <memory>
#include <cstring>

#include <QStandardPaths>
#include <QDir>
#include <QTemporaryDir>

#include <nlohmann/json.hpp>

// inline platform-specific macros to make the code look more readable
#if defined(_WIN32) || defined(_WIN64)
#define MVC_WIN(...) __VA_ARGS__
#define MVC_MAC(...)
#define MVC_LINUX(...)
#define MVC_UNIX(...)
#elif defined(__APPLE__)
#define MVC_WIN(...) 
#define MVC_MAC(...) __VA_ARGS__
#define MVC_LINUX(...)
#define MVC_UNIX(...) __VA_ARGS__
#elif defined(__linux__)
#define MVC_WIN(...) 
#define MVC_MAC(...)
#define MVC_LINUX(...) __VA_ARGS__
#define MVC_UNIX(...) __VA_ARGS__
#else
#error "what"
#endif

#ifdef _WIN32
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

fs::path get_appdata_dir() {
    return fs::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString());
}

// ============================================================================
// 2. LibbiVFS
// ============================================================================
void* fuse_init(struct fuse_conn_info *, struct fuse_config *cfg);

struct VFSStartupConfigs {
    const ModsIndex::Mod mod; 

    const std::string mountpoint;
};

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
    fs::path whiteoutFilePath;
    std::unordered_set<std::string> whiteouts;
    mutable std::shared_mutex dbMutex;

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
        if (!fs::exists(whiteoutFilePath)) return;

        std::ifstream f(whiteoutFilePath);
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) whiteouts.insert(normalize_path(line));
        }
    }

    void save_whiteouts() {
        std::unique_lock lock(dbMutex);
        fs::create_directories(dbFolder);
        std::ofstream f(whiteoutFilePath, std::ios::trunc);
        for (const auto &w : whiteouts) f << w << "\n";
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

    void add_whiteout(const std::string &path) {
        {
            std::unique_lock lock(dbMutex);
            whiteouts.insert(normalize_path(path));
        }
        save_whiteouts();
    }

    void remove_whiteout(const std::string &path) {
        {
            std::unique_lock lock(dbMutex);
            whiteouts.erase(normalize_path(path));
        }
        save_whiteouts();
    }

    fs::path launcherRoot;
    bool verboseLog = false;

public:
    LibbiVFS(std::string launcherRoot, decltype(startupConfigs) startupConfigs)
        : launcherRoot(launcherRoot), startupConfigs(startupConfigs) {

        auto settings = ProfileSettings::get();
        baseFolder = settings->value("baseGameInstallPath").toString().toStdString();
        if (baseFolder.empty()) {
            std::cerr << "Base game installation path not found in settings.json. Please set it in the launcher." << std::endl;
            throw std::runtime_error("Base game installation path not found.");
        }

        modFolder = startupConfigs.mod.modPath;

        containsRenpyEngine = fs::exists(modFolder / "lib");
        dbFolder = modFolder / ".vclubmgr";
        whiteoutFilePath = dbFolder / "whiteouts.txt";
        load_whiteouts();
    }

    void enableYapping() { verboseLog = true; }

    std::string get_path_2(const std::string &path, bool write = false) const {
        std::string stripped = strip_leading_slash(path);
        fs::path toModFolder = modFolder / stripped;
        fs::path toBaseFolder = baseFolder / stripped;

        if (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0) {
            auto patchesPath = launcherRoot / "patches" / path.substr((path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? 6 : 5);
            if (fs::exists(patchesPath) || (write && fs::exists(patchesPath.parent_path()))) {
                return patchesPath.string();
            }
        }

        if (path.rfind("/lib", 0) == 0 || path.rfind("\\lib", 0) == 0) {
            return (!containsRenpyEngine) ? toBaseFolder.string() : toModFolder.string();
        }

        if (write) return toModFolder.string();
        return fs::exists(toModFolder) ? toModFolder.string() : toBaseFolder.string();
    }

    std::string get_path(const std::string &path, bool write = false) const {
        auto ret = get_path_2(path, write);
        if (verboseLog) std::cout << "Requested " << path << " | Resolved to " << ret << std::endl;
        return ret;
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
        std::string stripped = strip_leading_slash(path);
        std::string mod_path = (!containsRenpyEngine && (path.rfind("/lib", 0) == 0 || path.rfind("\\lib", 0) == 0)) 
                             ? (baseFolder / stripped).string() : (modFolder / stripped).string();
        std::string base_path = (baseFolder / stripped).string();
        bool found = false;

        auto populate = [&](const fs::path &dir) {
            if (fs::is_directory(dir)) {
                found = true;
                for (const auto &entry : fs::directory_iterator(dir)) {
                    std::string name = entry.path().filename().string();
                    if (name != ".vclubmgr" && !is_whiteouted(path + "/" + name)) {
                        dirents.insert(name);
                    }
                }
            }
        };

        if (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0) {
            fs::path patchesSubdir = (launcherRoot / "patches") / path.substr((path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? 6 : 5);
            populate(patchesSubdir);
        }

        populate(mod_path);
        populate(base_path);

        if (!found) return -ENOENT;
        for (const auto &entry : dirents) filler(entry);
        return 0;
    }

    int open(const std::string &path, int flags, uint64_t &fh) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path);
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
#ifdef _WIN32
        try {
            std::string target = fs::read_symlink(full_path).string();
            size_t copy_len = (std::min)(size - 1, target.size());
            std::memcpy(buf, target.c_str(), copy_len);
            buf[copy_len] = '\0';
            return 0;
        } catch (...) { return -ENOENT; }
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
#ifdef _WIN32
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
        if (fs::exists(full_path) && !is_whiteouted(path)) return -EEXIST;

        remove_whiteout(path);
        fs::create_directories(fs::path(full_path).parent_path());
        int fd = open_file(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
        if (fd == -1) return -errno;
        close_file(fd);
        return 0;
    }

    int truncate(const std::string &path, off_t size) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path, true);
        if (!fs::exists(full_path)) {
            std::string base_path = get_path(path, false);
            if (!fs::exists(base_path)) return -ENOENT;
            fs::create_directories(fs::path(full_path).parent_path());
            fs::copy_file(base_path, full_path, fs::copy_options::overwrite_existing);
        }
#ifdef _WIN32
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
        if (fs::exists(full_path) && !is_whiteouted(path)) return -EEXIST;

        remove_whiteout(path);
        fs::create_directories(full_path);
        chmod_file(full_path.c_str(), mode);
        return 0;
    }

    int rmdir(const std::string &path) {
      if (is_whiteouted(path)) return -ENOENT;

      std::string mod_path = get_path(path, true);
      std::string base_path = (baseFolder / strip_leading_slash(path)).string();

      bool exists_in_mod = fs::exists(mod_path) && fs::is_directory(mod_path);
      bool exists_in_base = fs::exists(base_path) && fs::is_directory(base_path);

      if (!exists_in_mod && !exists_in_base) {
          return -ENOENT;
      }

      if (exists_in_mod) {
          fs::remove_all(mod_path);
      }

      if (exists_in_base && !should_bypass_whiteout(path)) {
          add_whiteout(path);
      }

      return 0;
    }

    int unlink(const std::string &path) {
      if (is_whiteouted(path)) return -ENOENT;

      std::string mod_path = get_path(path, true);
      std::string base_path = (baseFolder / strip_leading_slash(path)).string();

      bool exists_in_mod = fs::exists(mod_path) && !fs::is_directory(mod_path);
      bool exists_in_base = fs::exists(base_path) && !fs::is_directory(base_path);

      if (!exists_in_mod && !exists_in_base) {
          return -ENOENT;
      }

      if (exists_in_mod) {
          fs::remove(mod_path);
      }

      if (exists_in_base && !should_bypass_whiteout(path)) {
          add_whiteout(path);
      }

      return 0;
    }

    int rename(const std::string &oldpath, const std::string &newpath) {
        if (is_whiteouted(oldpath)) return -ENOENT;

        std::string old_full = get_path(oldpath, true);
        std::string new_full = get_path(newpath, true);

        if (!fs::exists(old_full)) {
            std::string old_base = get_path(oldpath, false);
            if (!fs::exists(old_base)) return -ENOENT;
            fs::create_directories(fs::path(old_full).parent_path());
            if (fs::is_directory(old_base)) fs::copy(old_base, old_full, fs::copy_options::recursive);
            else fs::copy_file(old_base, old_full);
        }

        fs::create_directories(fs::path(new_full).parent_path());
        if (::rename(old_full.c_str(), new_full.c_str()) == -1) return -errno;

        if (fs::exists((baseFolder / strip_leading_slash(oldpath)).string())) {
            add_whiteout(oldpath);
        }
        remove_whiteout(newpath);
        return 0;
    }

    int chmod(const std::string &path, mode_t mode) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path, true);
        if (!fs::exists(full_path)) return -ENOENT;
        return (chmod_file(full_path.c_str(), mode) == -1) ? -errno : 0;
    }

    int utimens(const std::string &path, const struct timespec tv[2]) {
        if (is_whiteouted(path)) return -ENOENT;
        std::string full_path = get_path(path, true);
        if (!fs::exists(full_path)) return -ENOENT;
#ifdef _WIN32
        try {
            auto file_time = std::chrono::system_clock::from_time_t(tv[1].tv_sec);
            fs::last_write_time(full_path, std::chrono::time_point_cast<fs::file_time_type::duration>(
                file_time - std::chrono::system_clock::now() + fs::file_time_type::clock::now()));
            return 0;
        } catch (...) { return -EIO; }
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

#ifdef _WIN32
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
#ifdef _WIN32
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
        fuse_opt_add_arg(&args, "kernel_cache,attr_timeout=10,entry_timeout=10,negative_timeout=2");

        fh = fuse_new(&args, &ops, sizeof(ops), vfs.get());
        fuse_opt_free_args(&args);
        if (!fh) return false;

        if (fuse_set_signal_handlers(fuse_get_session(fh)) != 0) {
            std::cerr << "Failed to install FUSE signal handlers\n";
        }

        if (fuse_mount(fh, mountpoint.toStdString().c_str()) != 0) {
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

        if (fh) fuse_exit(fh);
        auto* session = fh ? fuse_get_session(fh) : nullptr;
        auto loop = std::move(loopThread);
        auto handle = fh;
        lock.unlock();

        if (loop.joinable()) loop.join();

        lock.lock();
        if (handle) {
            fuse_remove_signal_handlers(session);
            fuse_unmount(handle);
            fuse_destroy(handle);
            fh = nullptr;
        }
        lock.unlock();

        try {
            fs::remove_all(mountpoint.toStdString());
        } catch (const std::exception& e) {
            std::cerr << "Failed to remove mount point: " << e.what() << '\n';
        }
    }

    QString mountPath() const {
        return mountpoint;
    }

private:
    static QString createMountPoint(const QString& modId) {
        QTemporaryDir directory(QDir::tempPath() + "/vclubmgr_mount_" + modId + "_XXXXXX");
        if (!directory.isValid()) {
            throw std::runtime_error("Failed to create temporary mount point");
        }
        directory.setAutoRemove(false);
        return directory.path();
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
