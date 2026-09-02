#ifndef RenpyArchive_H
#define RenpyArchive_H

#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
namespace py = pybind11;
namespace ModsIndex {
  void loadArchiveReaderModules();
  
  py::module_ rpaReaderModule();
  py::module_ rpycReaderModule();
}
#pragma pop_macro("slots")
#endif
