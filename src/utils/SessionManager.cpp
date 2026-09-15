#include "SessionManager.hpp"
#include <QCoreApplication>
#include <fcntl.h>
#include <functional>
#include <QDebug>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <memory>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
extern char** environ;
#endif

#include "macros.h"
#include <QApplication>

static std::unordered_map<std::string, std::unique_ptr<VFSInstance>> m_mountedMods;

void SessionManager::mount(ModsIndex::Mod& mod) {
  if (m_mountedMods.contains(mod.id)) return;

  auto& instance = m_mountedMods[mod.id] = std::make_unique<VFSInstance>(
    QCoreApplication::applicationFilePath(),
    mod
  );

  instance->mount();

  QApplication::setQuitOnLastWindowClosed(false);
};


namespace fs = std::filesystem;

void spawn_process(
    const std::string& command,
    const std::vector<std::string>& argv,
    const std::vector<std::string>& extra_env,
    std::function<void(pid_t pid)> on_created,
    std::function<void(int exit_code)> on_exit
)
{
    // Launch a background thread to block on child execution
    std::thread([command, argv, extra_env, on_created, on_exit]() {
        int exit_code = -1;

#if defined(_WIN32)
        // -------------------------------------------------------------
        // WINDOWS IMPLEMENTATION
        // -------------------------------------------------------------
        char* sys_env = GetEnvironmentStringsA();
        if (!sys_env) {
            if (on_exit) on_exit(-1);
            return;
        }

        std::vector<char> env_block;
        
        // 1. Copy existing system environment block
        char* ptr = sys_env;
        while (*ptr != '\0') {
            size_t len = std::strlen(ptr) + 1;
            env_block.insert(env_block.end(), ptr, ptr + len);
            ptr += len;
        }
        FreeEnvironmentStringsA(sys_env);

        // 2. Append extra "KEY=VALUE" strings
        for (const auto& var : extra_env) {
            env_block.insert(env_block.end(), var.c_str(), var.c_str() + var.length() + 1);
        }

        // 3. Add final double null terminator required by Windows
        env_block.push_back('\0');

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        // Wrap command in quotes to handle paths with spaces safely
        std::string cmd = "\"" + command + "\"";
        for (const auto& arg : argv) {
            cmd += " \"" + arg + "\"";
        }

        if (CreateProcessA(NULL, cmd.data(), NULL, NULL, FALSE, 0, env_block.data(), NULL, &si, &pi)) {
            if (on_created) on_created(pi.dwProcessId);
            // Block thread until child exits
            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD code = 0;
            if (GetExitCodeProcess(pi.hProcess, &code)) {
                exit_code = static_cast<int>(code);
            }

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

#else
        // -------------------------------------------------------------
        // LINUX / POSIX IMPLEMENTATION
        // -------------------------------------------------------------
        std::vector<std::string> env_storage;
        std::vector<char*> env_vec;

        // Parse key names from extra_env (e.g. "LD_LIBRARY_PATH")
        std::vector<std::string> extra_keys;
        for (const auto& var : extra_env) {
            auto pos = var.find('=');
            if (pos != std::string::npos) {
                extra_keys.push_back(var.substr(0, pos));
            }
        }

        // Copy existing environment except keys that are overridden in extra_env
        for (char** env = environ; *env != nullptr; ++env) {
            std::string entry(*env);
            auto pos = entry.find('=');
            std::string key = (pos != std::string::npos) ? entry.substr(0, pos) : entry;

            bool is_overridden = false;
            for (const auto& k : extra_keys) {
                if (k == key) { is_overridden = true; break; }
            }

            if (!is_overridden) {
                env_storage.push_back(entry);
            }
        }

        // Add extra environment variables
        for (const auto& var : extra_env) {
            env_storage.push_back(var);
        }

        for (auto& s : env_storage) {
            env_vec.push_back(s.data());
        }
        env_vec.push_back(nullptr);

        std::vector<char*> argv_vec;
        
        // FIX: argv[0] must be the command/executable name by POSIX convention
        argv_vec.push_back(const_cast<char*>(command.c_str()));
        
        for (const auto& arg : argv) {
            argv_vec.push_back(const_cast<char*>(arg.c_str()));
        }
        argv_vec.push_back(nullptr);

        pid_t pid;
        int status = posix_spawn(&pid, command.c_str(), NULL, NULL, argv_vec.data(), env_vec.data());

        if (status == 0) {
            if (on_created) on_created(pid);
            int wait_status = 0;
            // Block thread until process terminates
            if (waitpid(pid, &wait_status, 0) != -1) {
                if (WIFEXITED(wait_status)) {
                    exit_code = WEXITSTATUS(wait_status);
                } else if (WIFSIGNALED(wait_status)) {
                    exit_code = 128 + WTERMSIG(wait_status);
                }
            }
        }
#endif

        // Trigger callback on background thread upon exit
        if (on_exit) {
            on_exit(exit_code);
        }
    }).detach();
}


void SessionManager::launch(ModsIndex::Mod& mod, std::function<void()> exitedCallback, const QString& saveId) {
  mount(mod);

  // Step 1. get the bundled pythonw executable

  fs::path mountPath = m_mountedMods[mod.id]->mountPath().toStdString();
  std::optional<fs::path> pythonwPath;
  // mac
  #if defined(__APPLE__)
  {
      auto p = mountPath / (configs.modId+".app") / "Contents" / "MacOS" / "pythonw";
      if (fs::exists(p)) {
          pythonwPath = p;
      }
  }
  #else 
  //forgive me
      std::string platformKey =
          MVC_WIN("windows") MVC_LINUX("linux")
          "-"
      #if defined(__x86_64__) || defined(_M_X64)
          "x86_64"
      #elif defined(__i386__) || defined(_M_IX86)
          "i686"
      #endif
      ;

      std::string exeName = "pythonw" MVC_WIN(".exe");

      {
          auto py3Path = mountPath / "lib" / ("py3-" + platformKey) / exeName;
          if (fs::exists(py3Path)) {
              pythonwPath = py3Path;
          }
      }

      if (!pythonwPath) {
          auto py2Path = mountPath / "lib" / ("py2-" + platformKey) / exeName;
          if (fs::exists(py2Path)) {
              pythonwPath = py2Path;
          }
      }

      if (!pythonwPath) {
          auto directPath = mountPath / "lib" / platformKey / exeName;
          if (fs::exists(directPath)) {
              pythonwPath = directPath;
          }
      }

      #endif
  if (!pythonwPath) {
      throw std::runtime_error("Could not find pythonw executable for the current environment in the mod folder.");
  }

  // Step 1.5: Set executable permission on specifically Linux platform 
  #if defined(__linux__)
  {
      fs::permissions(*pythonwPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);
  }
  #endif

  // Step 2. Purge loose .rpyc files if forceRecompile is set
  if (mod.forceRecompile) {
      for (const auto& entry : fs::recursive_directory_iterator(mountPath)) {
          if (entry.is_regular_file() && entry.path().extension() == ".rpyc") {
              fs::remove(entry.path());
          }
      }
  }

  // Step 3. Resolve bootstrapper python file at the root
  const std::array<std::string, 3> bootstrapperFiles {mod.buildId+".py", "DDLC.py", "renpy.py"};
  std::string bootstrapperPath;
  for (const auto& file : bootstrapperFiles) {
      auto candidate = mountPath / file;
      if (fs::exists(candidate)) {
          bootstrapperPath = candidate.string();
          break;
      }
  }

  if (bootstrapperPath.empty()) {
      throw std::runtime_error("Could not find a bootstrapper python file in the mod folder.");
  }

  // Step 4. Prepare environment variables and launch the game
  std::vector<std::string> extra_env;
  std::vector<std::string> argv;
  extra_env.push_back("MVC_MOD_ID=" + mod.id);
  if (mod.enableDeveloper) extra_env.push_back("MVC_DEVELOPER=Mon-ika");
  if (saveId != "") extra_env.push_back(("MVC_SAVE_ID=" + saveId).toStdString());

  #ifdef __linux__
  // get current list of LD_LIBRARY_PATH
  const char* ld_library_path = std::getenv("LD_LIBRARY_PATH");
  extra_env.push_back("LD_LIBRARY_PATH=" + pythonwPath->parent_path().string() + ":" + (ld_library_path ? ld_library_path : ""));
  #endif

  // Ren'Py 6 fallback launch flag. cant believe i have to do this
  // TODO: this check's works but it's weird, given that we're going 
  // to allow users to omit the renpy engine from the mod folder. not now though so no care
  if (!fs::exists(fs::path(mod.modPath) / "lib")) {
      argv.push_back("-EO");
  }
  argv.push_back(bootstrapperPath);

  std::string modId = mod.id;

  spawn_process(
      pythonwPath->string(), argv, extra_env, 
      [](pid_t pid) {},
      [modId, exitedCallback](int exit_code) {
          qDebug() << "Game exited with code:" << exit_code;
          if (exit_code == 0) SessionManager::unmount(modId);
          exitedCallback();
      }
  );
}

void SessionManager::unmount(ModsIndex::Mod& mod) {unmount(mod.id);}
void SessionManager::unmount(std::string modId) {
  m_mountedMods[modId]->unmount();
  m_mountedMods.erase(modId);

  if (m_mountedMods.size() == 0) {
    QApplication::setQuitOnLastWindowClosed(true);
  }
};
