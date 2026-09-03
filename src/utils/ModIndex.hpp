#ifndef ModIndex_H
#define ModIndex_H

#include <filesystem>
#include <string>
#include <vector>
namespace ModsIndex {
  struct Mod {
    std::string id;
    std::string name;
    std::string version;
    std::string buildId;

    std::string modPath;

    bool enableDeveloper;


    std::string getIconPath() const;
  };
  void loadModsIndex();

  std::vector<Mod>& getMods();

  void installMod(std::filesystem::path path);
}

#endif
