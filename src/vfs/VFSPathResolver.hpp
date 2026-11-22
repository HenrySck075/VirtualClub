#pragma once

#include <filesystem>
#include <string>

class VFSPathResolver {
public:
  VFSPathResolver(std::filesystem::path baseFolder,
                  std::filesystem::path modFolder,
                  std::filesystem::path deltaFolder,
                  std::filesystem::path launcherRoot,
                  std::string modId,
                  bool containsRenpyEngine);

  std::string getPath(const std::string& path, bool write = false) const;

private:
  std::filesystem::path baseFolder;
  std::filesystem::path modFolder;
  std::filesystem::path deltaFolder;
  std::filesystem::path launcherRoot;
  std::string modId;
  bool containsRenpyEngine;

  std::string stripLeadingSlash(const std::string& path) const;
  std::string normalizePath(const std::string& path) const;
};
