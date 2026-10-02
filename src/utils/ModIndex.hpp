#ifndef ModIndex_H
#define ModIndex_H

#include <filesystem>
#include <string>
#include <vector>
#include "utils/YamlSettings.hpp"

class ModsIndex {
public:
  struct Mod {
    std::string id;
    std::string name;
    std::string version;
    std::string buildId;

    std::string modPath;

    bool enableDeveloper;
    bool forceRecompile;

    std::string getIconPath() const;

    using PlaytimePair = std::pair<std::chrono::seconds, std::chrono::seconds>;

    std::optional<PlaytimePair> getPlaytime() const;

    bool operator==(const Mod& other) const = default;
  };

private:
  std::shared_ptr<YamlSettings> m_store;
  std::vector<Mod> m_modsList;

  explicit ModsIndex(const QString &path);


public:
  ModsIndex() = delete;

  ~ModsIndex();
  static std::shared_ptr<ModsIndex> get();

  void addModToRecentlyPlayed(const std::string& id);
  const std::vector<Mod>& getRecentlyPlayed();

  const std::vector<Mod>& getMods();
  Mod installMod(std::filesystem::path path);
  void removeMod(Mod& mod);
  const Mod& getModByID(std::string id);
};

#endif
