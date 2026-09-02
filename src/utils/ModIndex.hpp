#ifndef ModIndex_H
#define ModIndex_H

#include <filesystem>
#include <string>
namespace ModsIndex {
  struct Mod {
    std::string id;
    std::string name;
    std::string version;
    std::string buildId;

    std::string modPath;

    bool enableDeveloper;
  };
  void loadModsIndex();


  void installMod(std::filesystem::path path);
}

#endif
