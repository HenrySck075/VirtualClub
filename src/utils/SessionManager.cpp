#include "SessionManager.hpp"
#include <QCoreApplication>
#include <fcntl.h>
#include <functional>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <string>
#include "macros.h"
#include "TrayIcon.hpp"
#include "vfs/vfs.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>

#ifndef __MINGW32__
typedef unsigned short mode_t;
typedef unsigned long pid_t;
typedef SSIZE_T ssize_t;
#endif
#else
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
extern char** environ;

#endif

#include <QApplication>

#ifdef MVC_VFS_AVAILABLE

#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
static std::unordered_map<std::string, std::unique_ptr<VFSInstance>> g_mountedMods;
static std::unordered_map<std::string, pid_t> g_playingMods;

#ifdef _WIN32
#include <shellapi.h>
#include <QMessageBox>

static bool isWinFspInstalled() {
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (scm) {
    SC_HANDLE service = OpenServiceW(scm, L"WinFsp.Launcher", SERVICE_QUERY_STATUS);
    if (service) {
      CloseServiceHandle(service);
      CloseServiceHandle(scm);
      return true;
    }
    CloseServiceHandle(scm);
  }

  const QStringList installRoots = {
    qEnvironmentVariable("ProgramFiles"),
    qEnvironmentVariable("ProgramW6432"),
    qEnvironmentVariable("ProgramFiles(x86)")
  };
  for (const QString& root : installRoots) {
    if (root.isEmpty()) continue;
    for (const wchar_t* dllName : {L"winfsp-x64.dll", L"winfsp-x86.dll"}) {
      const std::wstring dllPath =
        (root + "/WinFsp/bin/" + QString::fromWCharArray(dllName)).toStdWString();
      if (GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
      }
    }
  }
  return false;
}

static void ensureWinFspInstalled() {
  if (isWinFspInstalled()) return;

  const QString scriptPath = QDir(
    QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir()
  ).filePath("assets/scripts/install-winfsp.ps1");
  if (!QFileInfo::exists(scriptPath)) {
    throw std::runtime_error(
      "WinFsp is not installed and the bundled installer could not be found."
    );
  }

  const auto answer = QMessageBox::question(
    nullptr,
    QObject::tr("WinFsp is required"),
    QObject::tr(
      "Virtual Club needs the WinFsp driver to run mods with the virtual filesystem. "
      "Install WinFsp now?"
    ),
    QMessageBox::Yes | QMessageBox::No,
    QMessageBox::Yes
  );
  if (answer != QMessageBox::Yes) {
    throw std::runtime_error(
      "WinFsp is required to mount mods. Installation was cancelled."
    );
  }

  const std::wstring executable = L"powershell.exe";
  const std::wstring parameters =
    L"-NoProfile -ExecutionPolicy Bypass -File \"" +
    scriptPath.toStdWString() +
    L"\"";
  const HINSTANCE result = ShellExecuteW(
    nullptr,
    L"runas",
    executable.c_str(),
    parameters.c_str(),
    nullptr,
    SW_SHOWNORMAL
  );
  if (reinterpret_cast<INT_PTR>(result) <= 32) {
    throw std::runtime_error(
      "Could not start the WinFsp installer. Run "
      "assets/scripts/install-winfsp.ps1 manually."
    );
  }

  throw std::runtime_error(
    "WinFsp installation started. Complete the installer, then restart Virtual Club."
  );
}
#endif

void SessionManager::mount(ModsIndex::Mod& mod) {
  qDebug() << "SessionManager: mount requested for" << QString::fromStdString(mod.id)
           << QString::fromStdString(mod.name);
  if (g_mountedMods.contains(mod.id)) {
    
    return;
  }

#ifdef _WIN32
  ensureWinFspInstalled();
#endif

  auto& instance = g_mountedMods[mod.id] = std::make_unique<VFSInstance>(
    QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir().path(),
    mod
  );

  if (!instance->mount()) {
    g_mountedMods.erase(mod.id);
    throw std::runtime_error("Could not mount the mod virtual filesystem.");
  }
  qDebug() << "SessionManager: mounted mod" << QString::fromStdString(mod.id)
           << "at" << instance->mountPath();

  QApplication::setQuitOnLastWindowClosed(false);
  TrayIcon::updateTooltip();
};

bool SessionManager::isMounted(std::string modId) {
  const bool result = g_mountedMods.contains(modId);
  
  return result;
}
bool SessionManager::isPlaying(std::string modId) {
  return g_playingMods.contains(modId);
}
QString SessionManager::mountPathOf(std::string modId) {
  
  auto it = g_mountedMods.find(modId);
  if (it == g_mountedMods.end()) {
    qWarning() << "SessionManager: mount path requested for unmounted mod"
               << QString::fromStdString(modId);
    return {};
  }
  return it->second->mountPath();
}

namespace fs = std::filesystem;

void spawn_process(
    const QString& command,
    const QString& cwd,
    const QStringList& argv,
    const QStringList& extra_env,
    std::function<void(pid_t pid)> on_created,
    std::function<void(int exit_code)> on_exit
)
{
  
  QProcess* process = new QProcess();
  process->setProgram(command);
  process->setArguments(argv);
  process->setWorkingDirectory(cwd);
  auto env = QProcessEnvironment::systemEnvironment();

  for (const auto& envVar : extra_env) {
      auto parts = envVar.split('=');
      if (parts.size() == 2) {
          env.insert(parts[0], parts[1]);
      }
  }

  process->setProcessEnvironment(env);
  
  

  QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                   [process, on_exit](int exitCode, QProcess::ExitStatus exitStatus) {
                       if (exitStatus == QProcess::CrashExit) {
                           
                       }
                       on_exit(exitCode);
                       process->deleteLater();
                   });

  QObject::connect(process, &QProcess::started, [process, on_created]() {
      on_created(process->processId());
  });
  QObject::connect(process, &QProcess::errorOccurred,
                   [process, on_exit](QProcess::ProcessError error) {
                       if (error == QProcess::FailedToStart) {
                           qWarning() << "Could not start process:" << process->program()
                                      << process->errorString();
                           on_exit(-1);
                           process->deleteLater();
                       }
                   });
  process->start();
}


void SessionManager::launch(ModsIndex::Mod& mod, std::function<void()> startedCallback, std::function<void()> exitedCallback, const QString& saveId) {
  qDebug() << "SessionManager: launch requested for" << QString::fromStdString(mod.id)
           << QString::fromStdString(mod.name)
           << "saveId" << saveId
           << "developer" << mod.enableDeveloper
           << "forceRecompile" << mod.forceRecompile;
  mount(mod);

  // Step 1. get the bundled pythonw executable

  fs::path mountPath = g_mountedMods[mod.id]->mountPath().toStdString();
  

// on windows (winfsp): repeatedly check mountPath existence for a few seconds to wait for the directory to mount
  #ifdef Q_OS_WIN
  {
      int attempts = 0;
      while (!fs::exists(mountPath) && attempts < 10) {
          
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          ++attempts;
      }
      if (!fs::exists(mountPath)) {
          qWarning() << "SessionManager: mount path does not exist after waiting" << QString::fromStdString(mountPath.string());
          throw std::runtime_error("Mount path does not exist after waiting.");
      }
  }
  #endif

  std::optional<fs::path> pythonwPath;
  QString launchCommand;
  QStringList launchPrefixArgs;
  bool usesTranslationLayer = false;
  // mac
  #ifdef Q_OS_MACOS
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

      const std::array<std::string, 3> pythonDirectories {
          "py3-", "py2-", ""
      };

      for (const auto& prefix : pythonDirectories) {
          auto pyPath = mountPath / "lib" / (prefix + platformKey) / exeName;
          
          if (fs::exists(pyPath)) {
              
              pythonwPath = pyPath;
              break;
          }
      }
  #endif

#if !defined(_WIN32)
  if (!pythonwPath) {
      const std::array<std::string, 2> windowsPlatformKeys {
          "windows-x86_64", "windows-i686"
      };

      for (const auto& directory : pythonDirectories) {
          for (const auto& windowsPlatformKey : windowsPlatformKeys) {
              auto candidate = mountPath / "lib" /
                  (directory + windowsPlatformKey) / "pythonw.exe";
              if (fs::exists(candidate)) {
                  pythonwPath = candidate;
                  break;
              }
          }
          if (pythonwPath) break;
      }

      if (pythonwPath) {
          qDebug() << "SessionManager: selected Python executable"
                   << QString::fromStdString(pythonwPath->string());
          const std::array<QString, 2> translationLayers {"wine64", "wine"};
          for (const auto& translationLayer : translationLayers) {
              launchCommand = QStandardPaths::findExecutable(translationLayer);
              if (!launchCommand.isEmpty()) {
                  
                  usesTranslationLayer = true;
                  break;
              }
          }

          if (!usesTranslationLayer) {
              qWarning() << "SessionManager: Windows python found but Wine is unavailable";
              throw std::runtime_error(
                  "Could not find a Wine translation layer to launch the Windows pythonw executable."
              );
          }
          launchPrefixArgs << QString::fromStdString(pythonwPath->string());
      }
  }
#endif

  if (!pythonwPath) {
      qWarning() << "SessionManager: no bundled pythonw executable found";
      throw std::runtime_error("Could not find pythonw executable for the current environment in the mod folder.");
  }

  // Step 1.5: Set executable permission on specifically Linux platform 
  #if defined(__linux__) && !defined(_WIN32)
  {
      if (!usesTranslationLayer) {
          fs::permissions(*pythonwPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);
      }
  }
  #endif

  // Step 2. Purge loose .rpyc files if forceRecompile is set
  if (mod.forceRecompile) {
      
      size_t removed = 0;
      for (const auto& entry : fs::recursive_directory_iterator(mountPath)) {
          if (entry.is_regular_file() && entry.path().extension() == ".rpyc") {
              
              fs::remove(entry.path());
              ++removed;
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
      qWarning() << "SessionManager: no bootstrapper found for mod" << QString::fromStdString(mod.id);
      throw std::runtime_error("Could not find a bootstrapper python file in the mod folder.");
  }
  

  // Step 4. Prepare environment variables and launch the game
  QStringList extra_env;
  QStringList argv;
  extra_env << QString::fromStdString("MVC_MOD_ID=" + mod.id);
  extra_env << QString::fromStdString("MVC_APPDATA_PATH=" + QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString());

  if (mod.enableDeveloper) extra_env << "MVC_DEVELOPER=Mon-ika";
  if (saveId != "") extra_env << ("MVC_SAVE_ID=" + saveId);
  

  #ifdef __linux__
  // get current list of LD_LIBRARY_PATH
  if (!usesTranslationLayer) {
      const char* ld_library_path = std::getenv("LD_LIBRARY_PATH");
      extra_env << QString::fromStdString("LD_LIBRARY_PATH=" + pythonwPath->parent_path().string() + ":" + (ld_library_path ? ld_library_path : ""));
  }
  #endif

  // Ren'Py 6 fallback launch flag. cant believe i have to do this
  // TODO: this check's works but it's weird, given that we're going 
  // to allow users to omit the renpy engine from the mod folder. not now though so no care
  if (!fs::exists(fs::path(mod.modPath) / "lib")) {
      argv << "-EO";
  }
  argv << QString::fromStdString(bootstrapperPath);
  if (usesTranslationLayer) {
      argv = launchPrefixArgs + argv;
  }

  if (launchCommand.isEmpty()) {
      launchCommand = QString::fromStdString(pythonwPath->string());
  }
  

  std::string modId = mod.id;

  spawn_process(
      launchCommand, QString::fromStdString(mountPath.string()), argv, extra_env,
      [modId, startedCallback](pid_t pid) {
          
          g_playingMods[modId] = pid;
          ModsIndex::get()->addModToRecentlyPlayed(modId);
          TrayIcon::updateTooltip();
          startedCallback();
      },
      [modId, exitedCallback](int exit_code) {
          
          g_playingMods.erase(modId);
          if (exit_code == 0) SessionManager::unmount(modId);
          exitedCallback();
      }
  );
}

void SessionManager::unmount(ModsIndex::Mod& mod) {unmount(mod.id);}
void SessionManager::unmount(std::string modId) {
  
  auto it = g_mountedMods.find(modId);
  if (it == g_mountedMods.end()) {
    qWarning() << "SessionManager: cannot unmount unknown mod" << QString::fromStdString(modId);
    return;
  }

  // try to sigterm the process before unmounting
  auto playingIt = g_playingMods.find(modId);
  if (playingIt != g_playingMods.end()) {
    
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, playingIt->second);
    if (hProcess) {
        TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
    } else {
        qWarning() << "SessionManager: failed to open process for termination" << playingIt->second;
    }
#else
    kill(playingIt->second, SIGTERM);
#endif
  }

  it->second->unmount();
  g_mountedMods.erase(it);
  qDebug() << "SessionManager: unmounted" << QString::fromStdString(modId)
           << "remaining mounts" << g_mountedMods.size();

  if (g_mountedMods.size() == 0) {
    QApplication::setQuitOnLastWindowClosed(true);
  }

  TrayIcon::updateTooltip();
};
bool SessionManager::hasMountedMod() {
  return !g_mountedMods.empty();
}
bool SessionManager::hasPlayingMod() {
  return !g_playingMods.empty();
}

size_t SessionManager::countMountedMods() {
  return g_mountedMods.size();
}
size_t SessionManager::countPlayingMods() {
  return g_playingMods.size();
}

namespace {
  struct smAutoCleanup {
    ~smAutoCleanup() {
      for (auto& [modId, instance] : g_mountedMods) {
        instance->unmount();
      }
      g_mountedMods.clear();
      g_playingMods.clear();
    }
  } dummy;
}

#else

void SessionManager::mount(ModsIndex::Mod&) {}
void SessionManager::launch(ModsIndex::Mod&, std::function<void()>, std::function<void()>, const QString&) {} 
void SessionManager::unmount(ModsIndex::Mod&) {}
void SessionManager::unmount(std::string) {}

bool SessionManager::isMounted(std::string modId) {
  return false;
}
bool SessionManager::isPlaying(std::string modId) {
  return false;
}
QString SessionManager::mountPathOf(std::string modId) {
  return "";
}

bool SessionManager::hasMountedMod() {
  return false;
}
bool SessionManager::hasPlayingMod() {
  return false;
}
size_t SessionManager::countMountedMods() {
  return 0;
}
size_t SessionManager::countPlayingMods() {
  return 0;
}
#endif

