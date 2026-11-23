#include "VFSPathResolver.hpp"

#include <filesystem>

namespace fs = std::filesystem;

VFSPathResolver::VFSPathResolver(fs::path baseFolder,
                                 fs::path modFolder,
                                 fs::path deltaFolder,
                                 fs::path launcherRoot,
                                 std::string modId,
                                 bool containsRenpyEngine)
  : baseFolder(std::move(baseFolder)),
    modFolder(std::move(modFolder)),
    deltaFolder(std::move(deltaFolder)),
    launcherRoot(std::move(launcherRoot)),
    modId(std::move(modId)),
    containsRenpyEngine(containsRenpyEngine) {}

std::string VFSPathResolver::stripLeadingSlash(const std::string& path) const {
  if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
    return path.substr(1);
  }
  return path;
}

std::string VFSPathResolver::normalizePath(const std::string& path) const {
  std::string normalized = path;
  while (!normalized.empty() &&
         (normalized.back() == '/' || normalized.back() == '\\')) {
    normalized.pop_back();
  }
  if (normalized.empty()) return "/";
  if (normalized[0] != '/' && normalized[0] != '\\') return "/" + normalized;
  return normalized;
}

std::string VFSPathResolver::getPath(const std::string& path, bool write) const {
  const std::string normalized = normalizePath(path);
  const std::string stripped = stripLeadingSlash(normalized);
  const fs::path contentPath = deltaFolder / "content" / stripped;

  static constexpr const char* launcherPatches = "/game/_launcher_patches";
  static constexpr const char* launcherPatchesPrefix = "/game/_launcher_patches/";

  if (normalized.rfind(launcherPatches, 0) == 0) {
    const std::string suffix = normalized.rfind(launcherPatchesPrefix, 0) == 0
      ? normalized.substr(std::char_traits<char>::length(launcherPatchesPrefix))
      : normalized.substr(std::char_traits<char>::length(launcherPatches));
    fs::path patchesPath = launcherRoot / "assets" / "patches" / suffix;
    if (fs::exists(patchesPath) ||
        (write && fs::exists(patchesPath.parent_path()))) {
      return patchesPath.string();
    }
  }

  fs::path modPath = modFolder / stripped;

  if (normalized.rfind("/lib", 0) == 0) {
    if (write) return contentPath.string();
    if (fs::exists(contentPath)) return contentPath.string();
    return !containsRenpyEngine
      ? (baseFolder / stripped).string()
      : modPath.string();
  }

  if (write) return contentPath.string();
  if (fs::exists(contentPath)) return contentPath.string();
  return fs::exists(modPath) ? modPath.string() : (baseFolder / stripped).string();
}
