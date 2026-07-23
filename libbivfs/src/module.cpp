#include <ostream>
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <chrono>

typedef unsigned short mode_t;
typedef SSIZE_T ssize_t;

#ifndef O_BINARY
#define O_BINARY 0
#endif

// Fallback definition for Windows where POSIX statvfs isn't present
struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    unsigned long f_files;
    unsigned long f_ffree;
    unsigned long f_favail;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

inline int lstat(const char *path, struct stat *stbuf) {
    return ::stat(path, stbuf);
}

inline int open_file(const char *path, int flags, int mode = 0) {
    return ::_open(path, flags, mode);
}

inline int close_file(int fd) {
    return ::_close(fd);
}

inline int access_file(const char *path, int mode) {
    return ::_access(path, mode);
}

inline int chmod_file(const char *path, mode_t mode) {
    return ::_chmod(path, mode);
}

// Windows polyfills for POSIX pread and pwrite
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
#include <unistd.h>
#include <dirent.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif

inline int open_file(const char *path, int flags, int mode = 0) {
    return ::open(path, flags, mode);
}

inline int close_file(int fd) {
    return ::close(fd);
}

inline int access_file(const char *path, int mode) {
    return ::access(path, mode);
}

inline int chmod_file(const char *path, mode_t mode) {
    return ::chmod(path, mode);
}
#endif

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cerrno>
#include <thread>
#include <mutex>
#include <memory>
#include <algorithm>
#include <cstring>

namespace py = pybind11;
namespace fs = std::filesystem;

// ============================================================================
// 1. Core PythonFileSystem Interface
// ============================================================================

class PythonFileSystem {
public:
    virtual ~PythonFileSystem() = default;
    
    virtual int getattr(const std::string &path, struct stat *stbuf) { return -ENOSYS; }
    virtual int readdir(const std::string &path, py::object filler) { return -ENOSYS; }
    virtual int read(const std::string &path, char *buf, size_t size, off_t offset) { return -ENOSYS; }
    virtual int readlink(const std::string &path, char *buf, size_t size) { return -ENOSYS; }
    virtual int access(const std::string &path, int mask) { return -ENOSYS; }
    virtual int statfs(const std::string &path, struct statvfs *stbuf) { return -ENOSYS; }
    
    virtual int create(const std::string &path, mode_t mode) { return -ENOSYS; }
    virtual int write(const std::string &path, const char *buf, size_t size, off_t offset) { return -ENOSYS; }
    virtual int truncate(const std::string &path, off_t size) { return -ENOSYS; }
    virtual int mkdir(const std::string &path, mode_t mode) { return -ENOSYS; }
    virtual int rmdir(const std::string &path) { return -ENOSYS; }
    virtual int unlink(const std::string &path) { return -ENOSYS; }
    virtual int rename(const std::string &oldpath, const std::string &newpath) { return -ENOSYS; }
    virtual int chmod(const std::string &path, mode_t mode) { return -ENOSYS; }
    virtual int utimens(const std::string &path, const struct timespec tv[2]) { return -ENOSYS; }
};

// ============================================================================
// 2. Upgraded LibbiVFS with Whiteout Engine
// ============================================================================

class LibbiVFS : public PythonFileSystem {
private:
    fs::path baseFolder;
    fs::path modFolder;
    bool isRenpyGameFolder;
    bool containsRenpyEngine;

    fs::path dbFolder;
    fs::path whiteoutFilePath;
    std::unordered_set<std::string> whiteouts;
    std::mutex dbMutex;

    std::string strip_leading_slash(const std::string &path) const {
        if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
            return path.substr(1);
        }
        return path;
    }

    std::string normalize_path(const std::string &path) const {
        std::string p = path;
        while (!p.empty() && (p.back() == '/' || p.back() == '\\')) {
            p.pop_back();
        }
        if (p.empty()) return "/";
        if (p[0] != '/' && p[0] != '\\') return "/" + p;
        return p;
    }

    void load_whiteouts() {
        std::lock_guard<std::mutex> lock(dbMutex);
        whiteouts.clear();
        if (!fs::exists(whiteoutFilePath)) return;

        std::ifstream f(whiteoutFilePath);
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) {
                whiteouts.insert(normalize_path(line));
            }
        }
    }

    void save_whiteouts() {
        std::lock_guard<std::mutex> lock(dbMutex);
        fs::create_directories(dbFolder);
        std::ofstream f(whiteoutFilePath, std::ios::trunc);
        for (const auto &w : whiteouts) {
            f << w << "\n";
        }
    }

    bool is_whiteouted(const std::string &path) {
        std::lock_guard<std::mutex> lock(dbMutex);
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
            std::lock_guard<std::mutex> lock(dbMutex);
            whiteouts.insert(normalize_path(path));
        }
        save_whiteouts();
    }

    void remove_whiteout(const std::string &path) {
        {
            std::lock_guard<std::mutex> lock(dbMutex);
            whiteouts.erase(normalize_path(path));
        }
        save_whiteouts();
    }

    fs::path launcherRoot;
    bool verboseLog = false;

public:
    LibbiVFS(std::string launcherRoot, std::string base, std::string mod, bool is_renpy)
        : launcherRoot(launcherRoot), baseFolder(base), modFolder(mod), isRenpyGameFolder(is_renpy) {

        containsRenpyEngine = fs::exists(modFolder / "lib");
        
        dbFolder = modFolder / ".libbivfs";
        whiteoutFilePath = dbFolder / "whiteouts.txt";
        load_whiteouts();
    }

    void enableYapping() {
        verboseLog = true;
    }

    std::string get_path_2(const std::string &path, bool write = false) const {
        std::string stripped = strip_leading_slash(path);
        fs::path toModFolder = modFolder / stripped;
        fs::path toBaseFolder = baseFolder / stripped;

        if (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0) {
            auto patchesPath = launcherRoot / "patches" / path.substr((path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? 6 : 5);
            if (fs::exists(patchesPath)) {
                return patchesPath.string();
            }
            if (write && fs::exists(patchesPath.parent_path())) {
                return patchesPath.string();
            }
        }

        if (path.rfind("/lib", 0) == 0 || path.rfind("\\lib", 0) == 0) {
            if (isRenpyGameFolder || !containsRenpyEngine) {
                return toBaseFolder.string();
            } else {
                return toModFolder.string();
            }
        } else {
            if (write) {
                if (isRenpyGameFolder) {
                    if (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0) {
                        std::string sub = (path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? path.substr(6) : path.substr(5);
                        return (modFolder / sub).string();
                    } else {
                        return toBaseFolder.string();
                    }
                } else {
                    return toModFolder.string();
                }
            } else {
                fs::path ret;
                if (isRenpyGameFolder && (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0)) {
                    std::string sub = (path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? path.substr(6) : path.substr(5);
                    ret = modFolder / sub;
                } else {
                    ret = toModFolder;
                }
                
                if (!fs::exists(ret)) {
                    ret = toBaseFolder;
                }
                return ret.string();
            }
        }
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

    int getattr(const std::string &path, struct stat *stbuf) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path);
        if (lstat(full_path.c_str(), stbuf) == -1) {
            return -errno;
        }
        return 0;
    }

    int readdir(const std::string &path, py::object filler) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::unordered_set<std::string> dirents = {".", ".."};
        std::string stripped = strip_leading_slash(path);
        std::string mod_path;
        
        if (isRenpyGameFolder && (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0)) {
            std::string sub = (path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? path.substr(6) : path.substr(5);
            mod_path = (modFolder / sub).string();
        } else if (!containsRenpyEngine && (path.rfind("/lib", 0) == 0 || path.rfind("\\lib", 0) == 0)) {
            mod_path = (baseFolder / stripped).string();
        } else {
            mod_path = (modFolder / stripped).string();
        }

        std::string base_path = (baseFolder / stripped).string();
        bool found = false;
        
        if (path.rfind("/game", 0) == 0 || path.rfind("\\game", 0) == 0) {
            fs::path patchesPath = launcherRoot / "patches";
            fs::path patchesSubdir = patchesPath / path.substr((path.rfind("/game/", 0) == 0 || path.rfind("\\game\\", 0) == 0) ? 6 : 5);
            if (fs::is_directory(patchesSubdir)) {
                found = true;
                for (const auto &entry : fs::directory_iterator(patchesSubdir)) {
                    std::string name = entry.path().filename().string();
                    if (!is_whiteouted(path + "/" + name)) {
                        dirents.insert(name);
                    }
                }
            }
        }
        
        if (fs::is_directory(mod_path)) {
            found = true;
            for (const auto &entry : fs::directory_iterator(mod_path)) {
                std::string name = entry.path().filename().string();
                if (name != ".libbivfs" && !is_whiteouted(path + "/" + name)) {
                    dirents.insert(name);
                }
            }
        }

        if (fs::is_directory(base_path)) {
            found = true;
            for (const auto &entry : fs::directory_iterator(base_path)) {
                std::string name = entry.path().filename().string();
                if (!is_whiteouted(path + "/" + name)) {
                    dirents.insert(name);
                }
            }
        }

        if (!found) return -ENOENT;

        for (const auto &entry : dirents) {
            filler(entry);
        }
        return 0;
    }

    int read(const std::string &path, char *buf, size_t size, off_t offset) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path);
        int fd = open_file(full_path.c_str(), O_RDONLY | O_BINARY);
        if (fd == -1) return -errno;

        int res = static_cast<int>(pread(fd, buf, size, offset));
        close_file(fd);
        if (res == -1) return -errno;
        return res;
    }

    int readlink(const std::string &path, char *buf, size_t size) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path);
#ifdef _WIN32
        try {
            std::string target = fs::read_symlink(full_path).string();
            size_t copy_len = (std::min)(size - 1, target.size());
            std::memcpy(buf, target.c_str(), copy_len);
            buf[copy_len] = '\0';
            return 0;
        } catch (...) {
            return -ENOENT;
        }
#else
        ssize_t res = ::readlink(full_path.c_str(), buf, size - 1);
        if (res == -1) return -errno;
        buf[res] = '\0';
        return 0;
#endif
    }

    int access(const std::string &path, int mask) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path);
        if (access_file(full_path.c_str(), mask) == -1) {
            return -errno;
        }
        return 0;
    }

    int statfs(const std::string &path, struct statvfs *stbuf) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path);
#ifdef _WIN32
        ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceExA(full_path.c_str(), &freeBytes, &totalBytes, &totalFreeBytes)) {
            stbuf->f_bsize = 4096;
            stbuf->f_frsize = 4096;
            stbuf->f_blocks = totalBytes.QuadPart / 4096;
            stbuf->f_bfree = totalFreeBytes.QuadPart / 4096;
            stbuf->f_bavail = freeBytes.QuadPart / 4096;
            return 0;
        }
        return -EIO;
#else
        if (::statvfs(full_path.c_str(), stbuf) == -1) {
            return -errno;
        }
        return 0;
#endif
    }

    int create(const std::string &path, mode_t mode) override {
        std::string full_path = get_path(path, true);
        if (fs::exists(full_path) && !is_whiteouted(path)) return -EEXIST;

        remove_whiteout(path);
        fs::create_directories(fs::path(full_path).parent_path());
        int fd = open_file(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
        if (fd == -1) return -errno;
        close_file(fd);
        return 0;
    }

    int write(const std::string &path, const char *buf, size_t size, off_t offset) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path, true);

        if (!fs::exists(full_path)) {
            std::string base_path = get_path(path, false);
            if (fs::exists(base_path)) {
                fs::create_directories(fs::path(full_path).parent_path());
                fs::copy_file(base_path, full_path, fs::copy_options::overwrite_existing);
            } else {
                fs::create_directories(fs::path(full_path).parent_path());
                int fd = open_file(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
                if (fd != -1) close_file(fd);
            }
        }

        int fd = open_file(full_path.c_str(), O_WRONLY | O_BINARY);
        if (fd == -1) return -errno;

        int res = static_cast<int>(pwrite(fd, buf, size, offset));
        close_file(fd);
        if (res == -1) return -errno;
        return res;
    }

    int truncate(const std::string &path, off_t size) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path, true);
        if (!fs::exists(full_path)) {
            std::string base_path = get_path(path, false);
            if (fs::exists(base_path)) {
                fs::create_directories(fs::path(full_path).parent_path());
                fs::copy_file(base_path, full_path, fs::copy_options::overwrite_existing);
            } else {
                return -ENOENT;
            }
        }
#ifdef _WIN32
        int fd = open_file(full_path.c_str(), O_RDWR | O_BINARY);
        if (fd == -1) return -errno;
        int res = _chsize(fd, static_cast<long>(size));
        close_file(fd);
        if (res != 0) return -errno;
#else
        if (::truncate(full_path.c_str(), size) == -1) {
            return -errno;
        }
#endif
        return 0;
    }

    int mkdir(const std::string &path, mode_t mode) override {
        std::string full_path = get_path(path, true);
        if (fs::exists(full_path) && !is_whiteouted(path)) return -EEXIST;

        remove_whiteout(path);
        fs::create_directories(full_path);
        chmod_file(full_path.c_str(), mode);
        return 0;
    }

    int rmdir(const std::string &path) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string mod_path = get_path(path, true);
        std::string base_path = (baseFolder / strip_leading_slash(path)).string();
        
        bool physically_removed = false;
        bool existed_in_mod = fs::exists(mod_path) && fs::is_directory(mod_path);
        if (existed_in_mod) {
            fs::remove_all(mod_path);
            physically_removed = true;
        }

        if (existed_in_mod && fs::exists(base_path) && fs::is_directory(base_path)) {
            if (!should_bypass_whiteout(path)) {
                add_whiteout(path);
            }
            return 0;
        }

        return physically_removed ? 0 : -ENOENT;
    }

    int unlink(const std::string &path) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string mod_path = get_path(path, true);
        std::string base_path = (baseFolder / strip_leading_slash(path)).string();

        bool physically_removed = false;
        bool existed_in_mod = fs::exists(mod_path) && !fs::is_directory(mod_path);
        if (existed_in_mod) {
            fs::remove(mod_path);
            physically_removed = true;
        }

        if (existed_in_mod && fs::exists(base_path) && !fs::is_directory(base_path)) {
            if (!should_bypass_whiteout(path)) {
                add_whiteout(path);
            }
            return 0;
        }

        return physically_removed ? 0 : -ENOENT;
    }

    int rename(const std::string &oldpath, const std::string &newpath) override {
        if (is_whiteouted(oldpath)) return -ENOENT;

        std::string old_full = get_path(oldpath, true);
        std::string new_full = get_path(newpath, true);

        if (!fs::exists(old_full)) {
            std::string old_base = get_path(oldpath, false);
            if (fs::exists(old_base)) {
                fs::create_directories(fs::path(old_full).parent_path());
                if (fs::is_directory(old_base)) {
                    fs::copy(old_base, old_full, fs::copy_options::recursive);
                } else {
                    fs::copy_file(old_base, old_full);
                }
            } else {
                return -ENOENT;
            }
        }

        fs::create_directories(fs::path(new_full).parent_path());
        if (::rename(old_full.c_str(), new_full.c_str()) == -1) {
            return -errno;
        }

        std::string old_base = (baseFolder / strip_leading_slash(oldpath)).string();
        if (fs::exists(old_base)) {
            add_whiteout(oldpath);
        }
        remove_whiteout(newpath);

        return 0;
    }

    int chmod(const std::string &path, mode_t mode) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path, true);
        if (!fs::exists(full_path)) return -ENOENT;
        if (chmod_file(full_path.c_str(), mode) == -1) return -errno;
        return 0;
    }

    int utimens(const std::string &path, const struct timespec tv[2]) override {
        if (is_whiteouted(path)) return -ENOENT;

        std::string full_path = get_path(path, true);
        if (!fs::exists(full_path)) return -ENOENT;
#ifdef _WIN32
        try {
            auto file_time = std::chrono::system_clock::from_time_t(tv[1].tv_sec);
            fs::last_write_time(full_path, std::chrono::time_point_cast<fs::file_time_type::duration>(
                file_time - std::chrono::system_clock::now() + fs::file_time_type::clock::now()));
            return 0;
        } catch (...) {
            return -EIO;
        }
#else
        if (utimensat(AT_FDCWD, full_path.c_str(), tv, 0) == -1) {
            return -errno;
        }
        return 0;
#endif
    }
};

// ============================================================================
// 3. Upgraded FUSE3 C-to-C++ Binding Glue & Non-blocking Mount Manager
// ============================================================================

namespace FUSE_Glue {
    static PythonFileSystem* get_fs() {
        auto* ctx = fuse_get_context();
        if (!ctx || !ctx->private_data) {
            return nullptr; 
        }
        return static_cast<PythonFileSystem*>(ctx->private_data);
    }

#ifdef _WIN32
    static int getattr_glue(const char *path, fuse_stat *stbuf, struct fuse_file_info *fi) {
        struct stat std_st = {};
        int res = get_fs()->getattr(path, &std_st);
        if (res == 0) {
            std::memset(stbuf, 0, sizeof(*stbuf));
            stbuf->st_mode = std_st.st_mode;
            stbuf->st_nlink = std_st.st_nlink;
            stbuf->st_size = std_st.st_size;
            stbuf->st_atime = std_st.st_atime;
            stbuf->st_mtime = std_st.st_mtime;
            stbuf->st_ctime = std_st.st_ctime;
            stbuf->st_uid = std_st.st_uid;
            stbuf->st_gid = std_st.st_gid;
        }
        return res;
    }

    static int readdir_glue(const char *path, void *buf, fuse_fill_dir_t filler,
                            fuse_off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
        py::gil_scoped_acquire acquire;
        auto py_filler = [buf, filler](const std::string &name) {
            filler(buf, name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        };
        return get_fs()->readdir(path, py::cpp_function(py_filler));
    }

    static int read_glue(const char *path, char *buf, size_t size, fuse_off_t offset, struct fuse_file_info *fi) {
        return get_fs()->read(path, buf, size, static_cast<off_t>(offset));
    }

    static int readlink_glue(const char *path, char *buf, size_t size) {
        return get_fs()->readlink(path, buf, size);
    }

    static int access_glue(const char *path, int mask) {
        return get_fs()->access(path, mask);
    }

    static int statfs_glue(const char *path, fuse_statvfs *stbuf) {
        struct statvfs std_st = {};
        int res = get_fs()->statfs(path, &std_st);
        if (res == 0) {
            std::memset(stbuf, 0, sizeof(*stbuf));
            stbuf->f_bsize = std_st.f_bsize;
            stbuf->f_frsize = std_st.f_frsize;
            stbuf->f_blocks = std_st.f_blocks;
            stbuf->f_bfree = std_st.f_bfree;
            stbuf->f_bavail = std_st.f_bavail;
            stbuf->f_files = std_st.f_files;
            stbuf->f_ffree = std_st.f_ffree;
            stbuf->f_favail = std_st.f_favail;
            stbuf->f_fsid = std_st.f_fsid;
            stbuf->f_flag = std_st.f_flag;
            stbuf->f_namemax = std_st.f_namemax;
        }
        return res;
    }

    static int create_glue(const char *path, fuse_mode_t mode, struct fuse_file_info *fi) {
        return get_fs()->create(path, static_cast<mode_t>(mode));
    }

    static int write_glue(const char *path, const char *buf, size_t size, fuse_off_t offset, struct fuse_file_info *fi) {
        return get_fs()->write(path, buf, size, static_cast<off_t>(offset));
    }

    static int truncate_glue(const char *path, fuse_off_t size, struct fuse_file_info *fi) {
        return get_fs()->truncate(path, static_cast<off_t>(size));
    }

    static int mkdir_glue(const char *path, fuse_mode_t mode) {
        return get_fs()->mkdir(path, static_cast<mode_t>(mode));
    }

    static int rmdir_glue(const char *path) {
        return get_fs()->rmdir(path);
    }

    static int unlink_glue(const char *path) {
        return get_fs()->unlink(path);
    }

    static int rename_glue(const char *oldpath, const char *newpath, unsigned int flags) {
        return get_fs()->rename(oldpath, newpath);
    }

    static int chmod_glue(const char *path, fuse_mode_t mode, struct fuse_file_info *fi) {
        return get_fs()->chmod(path, static_cast<mode_t>(mode));
    }

    static int utimens_glue(const char *path, const struct fuse_timespec tv[2], struct fuse_file_info *fi) {
        struct timespec std_tv[2];
        if (tv) {
            std_tv[0].tv_sec = static_cast<time_t>(tv[0].tv_sec);
            std_tv[0].tv_nsec = static_cast<long>(tv[0].tv_nsec);
            std_tv[1].tv_sec = static_cast<time_t>(tv[1].tv_sec);
            std_tv[1].tv_nsec = static_cast<long>(tv[1].tv_nsec);
            return get_fs()->utimens(path, std_tv);
        }
        return get_fs()->utimens(path, nullptr);
    }
#else
    static int getattr_glue(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
        return get_fs()->getattr(path, stbuf);
    }

    static int readdir_glue(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
        py::gil_scoped_acquire acquire;
        auto py_filler = [buf, filler](const std::string &name) {
            filler(buf, name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        };
        return get_fs()->readdir(path, py::cpp_function(py_filler));
    }

    static int read_glue(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
        return get_fs()->read(path, buf, size, offset);
    }

    static int readlink_glue(const char *path, char *buf, size_t size) {
        return get_fs()->readlink(path, buf, size);
    }

    static int access_glue(const char *path, int mask) {
        return get_fs()->access(path, mask);
    }

    static int statfs_glue(const char *path, struct statvfs *stbuf) {
        return get_fs()->statfs(path, stbuf);
    }

    static int create_glue(const char *path, mode_t mode, struct fuse_file_info *fi) {
        return get_fs()->create(path, mode);
    }

    static int write_glue(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
        return get_fs()->write(path, buf, size, offset);
    }

    static int truncate_glue(const char *path, off_t size, struct fuse_file_info *fi) {
        return get_fs()->truncate(path, size);
    }

    static int mkdir_glue(const char *path, mode_t mode) {
        return get_fs()->mkdir(path, mode);
    }

    static int rmdir_glue(const char *path) {
        return get_fs()->rmdir(path);
    }

    static int unlink_glue(const char *path) {
        return get_fs()->unlink(path);
    }

    static int rename_glue(const char *oldpath, const char *newpath, unsigned int flags) {
        return get_fs()->rename(oldpath, newpath);
    }

    static int chmod_glue(const char *path, mode_t mode, struct fuse_file_info *fi) {
        return get_fs()->chmod(path, mode);
    }

    static int utimens_glue(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
        return get_fs()->utimens(path, tv);
    }
#endif

    static struct fuse_operations get_ops() {
        struct fuse_operations ops = {};
        ops.getattr  = getattr_glue;
        ops.readdir  = readdir_glue;
        ops.read     = read_glue;
        ops.readlink = readlink_glue;
        ops.access   = access_glue;
        ops.statfs   = statfs_glue;
        ops.create   = create_glue;
        ops.write    = write_glue;
        ops.truncate = truncate_glue;
        ops.mkdir    = mkdir_glue;
        ops.rmdir    = rmdir_glue;
        ops.unlink   = unlink_glue;
        ops.rename   = rename_glue;
        ops.chmod    = chmod_glue;
        ops.utimens  = utimens_glue;
        return ops;
    }
}

class ActiveMount {
private:
    std::thread runThread;
    struct fuse* fh = nullptr;
    std::string mountpoint;
    py::object keptAliveFsReference;
public:
    ActiveMount() = default;
    ~ActiveMount() {
        unmount();
    }

    bool mountNonBlocking(const std::string &m_point, py::object &fs) {
        mountpoint = m_point;
        keptAliveFsReference = fs;
        struct fuse_operations ops = FUSE_Glue::get_ops();

        struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);
        fuse_opt_add_arg(&args, "libbi_fuse");

        fh = fuse_new(&args, &ops, sizeof(ops), fs.cast<PythonFileSystem*>());
        fuse_opt_free_args(&args);

        if (!fh) {
            return false;
        }

        if (fuse_mount(fh, mountpoint.c_str()) != 0) {
            fuse_destroy(fh);
            fh = nullptr;
            return false;
        }

        runThread = std::thread([this]() {
            fuse_loop(fh);
        });

        return true;
    }

    void unmount() {
        if (fh) {
            fuse_exit(fh);
            fuse_unmount(fh);
            
            if (runThread.joinable()) {
                runThread.join();
            }
            
            fuse_destroy(fh);
            fh = nullptr;
        }
    }
};

// ============================================================================
// 4. Module Declaration
// ============================================================================

PYBIND11_MODULE(libbifuse, m) {
    m.doc() = "mao";

    py::class_<PythonFileSystem>(m, "PythonFileSystem");

    py::class_<LibbiVFS, PythonFileSystem>(m, "LibbiVFS")
        .def(py::init<std::string, std::string, std::string, bool>(),
             py::arg("launcherRoot"), py::arg("baseFolder"), py::arg("modFolder"), py::arg("isRenpyGameFolder"))
        .def("getPath", &LibbiVFS::get_path, py::arg("path"), py::arg("write") = false)
        .def("enableYapping", &LibbiVFS::enableYapping);

    py::class_<ActiveMount>(m, "ActiveMount")
        .def(py::init<>())
        .def("mount", &ActiveMount::mountNonBlocking, py::arg("mountpoint"), py::arg("fs"))
        .def("unmount", &ActiveMount::unmount);
}
