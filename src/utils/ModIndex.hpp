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
    bool forceRecompile;

    std::string getIconPath() const;

    bool operator==(const Mod& other) const = default;
  };

  void loadModsIndex();

  std::vector<Mod>& getMods();
  Mod installMod(std::filesystem::path path);
  void removeMod(Mod& mod);
  const Mod& getModByID(std::string id);
}

#endif
