#ifndef RenpyArchive_H
#define RenpyArchive_H

#pragma push_macro("slots")
#undef slots
#include <filesystem>
#include <string>
#include <vector>
#include <pybind11/embed.h>
namespace py = pybind11;
namespace ArchiveReader {
  void loadArchiveReaderModules();
  
  py::module_ rpaReaderModule();
  py::module_ rpycReaderModule();
  py::object readGameFile(const std::vector<std::filesystem::path>& gameDirs,
                          const std::string& filepath);
  std::string readBuildId(const std::filesystem::path& gameDir);
}
#pragma pop_macro("slots")
#endif
