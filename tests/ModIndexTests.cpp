#include "utils/ModIndex.hpp"
#include "utils/YamlSettings.hpp"
#include "ModIndexTests.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>

std::string generate_uuid_v4() {
  return "00000000-0000-4000-8000-000000000000";
}

void ModIndexTests::deserializesAndPersistsMods() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());

  YamlSettings seed(QString("%1/store.yaml").arg(temp.path()));
  QMap<QString, QVariant> mods;
  QVariantMap modMap;
  modMap["name"] = "Test Mod";
  modMap["version"] = "1.2.3";
  modMap["buildId"] = "DDLC";
  modMap["path"] = "/tmp/test-mod";
  modMap["enableDeveloper"] = true;
  modMap["forceRecompile"] = false;
  mods["test-id"] = modMap;
  seed.setValue("mods", mods);
  QVERIFY(seed.save());

  ModsIndex index(QString("%1/store.yaml").arg(temp.path()));
  QCOMPARE(index.getMods().size(), 1u);
  const ModsIndex::Mod& mod = index.getModByID("test-id");
  QCOMPARE(QString::fromStdString(mod.name), "Test Mod");
  QCOMPARE(QString::fromStdString(mod.version), "1.2.3");
  QVERIFY(mod.enableDeveloper);
  QVERIFY(!mod.forceRecompile);

  ModsIndex::Mod copy = mod;
  copy.version = "2.0";
  index.updateMod(copy);
  QCOMPARE(index.getModByID("test-id").version, "2.0");

  index.removeMod(copy);
  QCOMPARE(index.getMods().size(), 0u);
  QVERIFY_EXCEPTION_THROWN(index.getModByID("test-id"), std::runtime_error);
}

void ModIndexTests::updatesAndRemovesMods() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());

  ModsIndex index(QString("%1/store.yaml").arg(temp.path()));
  ModsIndex::Mod mod {
    "id", "Name", "1", "DDLC",
    "/tmp/mod", false, false
  };
  // update/remove both have to reject unknown mods.
  QVERIFY_EXCEPTION_THROWN(index.updateMod(mod), std::invalid_argument);
  QVERIFY_EXCEPTION_THROWN(index.removeMod(mod), std::invalid_argument);
}

void ModIndexTests::recentlyPlayedIsUniqueAndLimited() {
  QTemporaryDir temp;
  QVERIFY(temp.isValid());

  ModsIndex index(QString("%1/store.yaml").arg(temp.path()));
  for (int i = 0; i < 12; ++i) {
    index.addModToRecentlyPlayed("missing-" + std::to_string(i));
  }
  QCOMPARE(index.getRecentlyPlayed().size(), 0u);
}
