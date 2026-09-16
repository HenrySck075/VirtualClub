#pragma once

#include "utils/ModIndex.hpp"
#include "vfs/vfs.hpp"
#include <unordered_map>

class SessionManager {
public:
  static void mount(ModsIndex::Mod& mod);
  static void launch(ModsIndex::Mod& mod, std::function<void()> exitedCallback, const QString& saveId = "");
  static void unmount(ModsIndex::Mod& mod);
  static void unmount(std::string modId);

  static bool isMounted(std::string modId);
  static QString mountPathOf(std::string modId);
};
