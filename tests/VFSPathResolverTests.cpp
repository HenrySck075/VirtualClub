#include "vfs/VFSPathResolver.hpp"
#include "VFSPathResolverTests.hpp"
#include "utils/ProfileSettings.hpp"
#include "vfs/vfs.hpp"
#include "consts.hpp"
#include "utils/macros.h"

#ifndef QT_TESTCASE_SOURCEDIR
#define QT_TESTCASE_SOURCEDIR "."
#endif

#include <QDir>
#include <QTemporaryDir>
#include <QSettings>
#include <QtTest>

#include <fstream>

void VFSPathResolverTests::modOverridesBaseAndWriteUsesDelta() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto root = std::filesystem::path(temp.path().toStdString());
  std::filesystem::create_directories(root / "base/game");
  std::filesystem::create_directories(root / "mod/game");
  std::filesystem::create_directories(root / "delta/content");
  std::ofstream(root / "base/game/shared.rpy") << "base";
  std::ofstream(root / "mod/game/shared.rpy") << "mod";

  VFSPathResolver resolver(root / "base", root / "mod", root / "delta",
                           root / "launcher", "test-mod", true);
  QCOMPARE(resolver.getPath("/game/shared.rpy"),
           (root / "mod/game/shared.rpy").string());
  QCOMPARE(resolver.getPath("/game/shared.rpy", true),
           (root / "delta/content/game/shared.rpy").string());
}

void VFSPathResolverTests::baseIsFallback() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto root = std::filesystem::path(temp.path().toStdString());
  std::filesystem::create_directories(root / "base/game");
  std::filesystem::create_directories(root / "mod/game");
  std::ofstream(root / "base/game/base-only.rpy") << "base";

  VFSPathResolver resolver(root / "base", root / "mod", root / "delta",
                           root / "launcher", "test-mod", true);
  QCOMPARE(resolver.getPath("/game/base-only.rpy"),
           (root / "base/game/base-only.rpy").string());
}

void VFSPathResolverTests::libUsesModOrBaseEngine() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto root = std::filesystem::path(temp.path().toStdString());
  std::filesystem::create_directories(root / "base/lib");
  std::filesystem::create_directories(root / "mod/lib");

  VFSPathResolver modEngine(root / "base", root / "mod", root / "delta",
                            root / "launcher", "test-mod", true);
  QCOMPARE(modEngine.getPath("/lib/pythonw"),
           (root / "mod/lib/pythonw").string());

  VFSPathResolver baseEngine(root / "base", root / "mod", root / "delta",
                             root / "launcher", "test-mod", false);
  QCOMPARE(baseEngine.getPath("/lib/pythonw"),
           (root / "base/lib/pythonw").string());
}

void VFSPathResolverTests::launcherPatchOverridesAllLayers() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto root = std::filesystem::path(temp.path().toStdString());
  std::filesystem::create_directories(root / "launcher/assets/patches/game");
  std::ofstream(root / "launcher/assets/patches/game/patched.rpy") << "patch";

  VFSPathResolver resolver(root / "base", root / "mod", root / "delta",
                           root / "launcher", "test-mod", true);
  QCOMPARE(resolver.getPath("/game/_launcher_patches/game/patched.rpy"),
           (root / "launcher/assets/patches/game/patched.rpy").string());
}

void VFSPathResolverTests::mountAndUnmount() {
#ifndef MVC_VFS_AVAILABLE
  QSKIP("VFS mounting is not available on this platform.");
#else
  QTemporaryDir storage;
  QVERIFY(storage.isValid());

  const QString sourceRoot = QString::fromUtf8(QT_TESTCASE_SOURCEDIR);
  const QString basePath = QDir::cleanPath(
    QString("%1/tests/subject/base").arg(sourceRoot));
  const QString modPath = QDir::cleanPath(
    QString("%1/tests/subject/mod").arg(sourceRoot));
  const QString launcherRoot = QDir::cleanPath(sourceRoot);

  auto profile = ProfileSettings::getOf("test");
  profile->setValue(STK_BASEPATH, basePath);
  profile->save();

  QSettings settings;
  settings.setValue(GSTK_STORAGE_PATH, storage.path());

  ModsIndex::Mod mod {
    "mount-test",
    "Mount Test",
    "1.0",
    "DDLC",
    modPath.toStdString(),
    false,
    false
  };

  VFSInstance vfs(launcherRoot, mod);
  QVERIFY(vfs.mount());
  QVERIFY(!vfs.mountPath().isEmpty());
  QVERIFY(QDir(vfs.mountPath()).exists());

  vfs.unmount();
  QVERIFY(!QDir(vfs.mountPath()).exists());
#endif
}
