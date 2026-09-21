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

#include <QApplication>

#ifdef MVC_VFS_AVAILABLE
static std::unordered_map<std::string, std::unique_ptr<VFSInstance>> m_mountedMods;

void SessionManager::mount(ModsIndex::Mod& mod) {
  if (m_mountedMods.contains(mod.id)) return;

  auto& instance = m_mountedMods[mod.id] = std::make_unique<VFSInstance>(
    QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir().path(),
    mod
  );

  instance->mount();

  QApplication::setQuitOnLastWindowClosed(false);
};

bool SessionManager::isMounted(std::string modId) {
  return m_mountedMods.contains(modId);
}
QString SessionManager::mountPathOf(std::string modId) {
  return m_mountedMods[modId]->mountPath();
}

namespace fs = std::filesystem;

void spawn_process(
    const QString& command,
    const QStringList& argv,
    const QStringList& extra_env,
    std::function<void(pid_t pid)> on_created,
    std::function<void(int exit_code)> on_exit
)
{
  QProcess* process = new QProcess();
  process->setProgram(command);
  process->setArguments(argv);
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
                           qDebug() << "Process crashed";
                       }
                       on_exit(exitCode);
                       process->deleteLater();
                   });

  process->start();
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
  QStringList extra_env;
  QStringList argv;
  extra_env << QString::fromStdString("MVC_MOD_ID=" + mod.id);
  if (mod.enableDeveloper) extra_env << "MVC_DEVELOPER=Mon-ika";
  if (saveId != "") extra_env << ("MVC_SAVE_ID=" + saveId);

  #ifdef __linux__
  // get current list of LD_LIBRARY_PATH
  const char* ld_library_path = std::getenv("LD_LIBRARY_PATH");
  extra_env << QString::fromStdString("LD_LIBRARY_PATH=" + pythonwPath->parent_path().string() + ":" + (ld_library_path ? ld_library_path : ""));
  #endif

  // Ren'Py 6 fallback launch flag. cant believe i have to do this
  // TODO: this check's works but it's weird, given that we're going 
  // to allow users to omit the renpy engine from the mod folder. not now though so no care
  if (!fs::exists(fs::path(mod.modPath) / "lib")) {
      argv << "-EO";
  }
  argv << QString::fromStdString(bootstrapperPath);

  std::string modId = mod.id;

  spawn_process(
      QString::fromStdString(pythonwPath->string()), argv, extra_env, 
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

#else
void SessionManager::mount(ModsIndex::Mod&) {}
void SessionManager::launch(ModsIndex::Mod&, std::function<void()>, const QString&) {} 
void SessionManager::unmount(ModsIndex::Mod&) {}
void SessionManager::unmount(std::string) {}

bool SessionManager::isMounted(std::string modId) {
  return false;
}
QString SessionManager::mountPathOf(std::string modId) {
  return "";
}

#endif

