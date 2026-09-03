#include "ModIndex.hpp"
#include <QSettings>
#include <filesystem>
#include <random>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <QStandardPaths>
#include "RenpyArchive.hpp"
#include "macros.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "../third_party/stb/stb_image_resize2.h"


py::bytes read_file_to_bytes(const std::string& filepath) {
    // Open in binary mode and position at the end to get file size
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read raw bytes into a standard buffer
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("Failed to read file contents: " + filepath);
    }

    // Pass buffer pointer and size to py::bytes
    // This copies the C++ memory buffer into a Python bytes object
    return py::bytes(buffer.data(), buffer.size());
}


std::string generate_uuid_v4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;

    uint32_t data[4] = { dis(gen), dis(gen), dis(gen), dis(gen) };

    // Set UUID version 4 (bits 12-15 of time_hi_and_version to 0100)
    data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
    // Set UUID variant (bits 6-7 of clock_seq_hi_and_reserved to 10)
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << data[0] << "-"
       << std::setw(4) << (data[1] >> 16) << "-"
       << std::setw(4) << (data[1] & 0xFFFF) << "-"
       << std::setw(4) << (data[2] >> 16) << "-"
       << std::setw(4) << (data[2] & 0xFFFF)
       << std::setw(8) << data[3];

    return ss.str();
}
namespace ModsIndex {
  std::vector<Mod> modsList;

  std::vector<Mod>& getMods() {
    return modsList;
  }

  void loadModsIndex() {
    modsList.clear();
    QSettings settings;

    // first pass: get all the mod ids
    std::vector<std::string> modIds;

    int size = settings.beginReadArray("mods");
    for (int i = 0; i < size; ++i) {
      settings.setArrayIndex(i);
      QString modId = settings.value("id").toString();
      modIds.push_back(modId.toStdString());  
    } 
    settings.endArray();

    modsList.reserve(modIds.size());
    // second pass: get the metadata
    // yeah yeah ik i can just put these in the mods array but its for easy editability
#define stringValue(...) value(__VA_ARGS__).toString().toStdString()
#define boolValue(...) value(__VA_ARGS__).toBool()
    for (auto modId : modIds) {
      Mod mod {
        modId,
        settings.stringValue(modId+"/name"),
        settings.stringValue(modId+"/version"),
        settings.stringValue(modId+"/buildId"),
        settings.stringValue(modId+"/path"),
        settings.boolValue(modId+"/enableDeveloper")
      };
      modsList.push_back(mod);
    }
  }
  void saveModsIndex() {
    QSettings settings;
    settings.beginWriteArray("mods", modsList.size());
    for (int i = 0; i < modsList.size(); ++i) {
      settings.setArrayIndex(i);
      const Mod& mod = modsList[i];
      settings.setValue("id", QString::fromStdString(mod.id));
    }
    settings.endArray();
    for (auto& mod : modsList) {
      settings.setValue(mod.id+"/name", QString::fromStdString(mod.name));
      settings.setValue(mod.id+"/version", QString::fromStdString(mod.version));
      settings.setValue(mod.id+"/buildId", QString::fromStdString(mod.buildId));
      settings.setValue(mod.id+"/path", QString::fromStdString(mod.modPath));
      settings.setValue(mod.id+"/enableDeveloper", mod.enableDeveloper);
    }
  }

  void writeIcons(std::string id, unsigned char* buffer, int len) {
    int x,y,comp;
    unsigned char* content = stbi_load_from_memory(buffer, len, &x, &y, &comp, 0);
    
    if (!content) {
      throw std::invalid_argument(std::string("Failed to decode image: ") + stbi_failure_reason());
    }

    static auto iconsDir = std::filesystem::path(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
    ) / "icons";

    // save the (.png'd) original under {id}.png
    // and a 80x80 downscaled version under {id}.scaled.png

    if (!std::filesystem::exists(iconsDir)) {
      std::filesystem::create_directories(iconsDir);
    }

    std::filesystem::path originalPath = iconsDir / (id + ".png");
    std::filesystem::path scaledPath = iconsDir / (id + ".scaled.png");

    stbi_write_png(originalPath.string().c_str(), x, y, comp, content, x * comp);
    unsigned char* scaledContent = new unsigned char[80 * 80 * comp];
    stbir_resize_uint8_linear(content, x, y, 0, scaledContent, 80, 80, 0, (stbir_pixel_layout)(comp));
    stbi_write_png(scaledPath.string().c_str(), 80, 80, comp, scaledContent, 80 * comp);

    stbi_image_free(content);
    delete[] scaledContent;
  }
  void writeIconsFromFile(std::string id, std::string file) {
    std::ifstream inputFile(file, std::ios::binary);
    if (!inputFile) {
        throw std::runtime_error("Could not open file: " + file);
    }
    auto fileSize = std::filesystem::file_size(file);
    std::vector<unsigned char> buffer(fileSize);
    inputFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    writeIcons(id, buffer.data(), fileSize);
  }

  std::string Mod::getIconPath() const {
    static auto iconsDir = std::filesystem::path(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
    ) / "icons";
    return (iconsDir / (id + ".png")).string();
  };

  void installMod(std::filesystem::path path) {
    static QSettings settings;
    auto modGameDir = path / "game";
    auto baseGameDir = std::filesystem::path(settings.stringValue("baseGameInstallPath")) / "game";
    std::unordered_map<std::string, py::object> indexes; 

    for (auto& dir : {baseGameDir, modGameDir}) {
      for (auto& p : std::filesystem::recursive_directory_iterator(dir)) {
        if (p.is_regular_file() && p.path().extension() == ".rpa") {
          auto filename = p.path().string();
          auto index = ModsIndex::rpaReaderModule().attr("read_rpa_index")(filename);
          indexes[filename] = index;
        }
      }
    };

    // returns a py::bytes or a none equivalent idr what its called
    auto readGameFile = [&modGameDir, &indexes](std::string filepath) -> py::object {
      auto modFilePath = modGameDir / filepath;
      if (std::filesystem::exists(modFilePath)) {
        return read_file_to_bytes(modFilePath.string());
      } else {
        for (auto& [key,value] : indexes) {
          auto index = value;
          if (index.contains(filepath.c_str())) {
            auto fileData = ModsIndex::rpaReaderModule().attr("extract_single_file")(key, filepath, index);
            return fileData;
          }
        }
      }
      return py::none{};
    };

    auto optionsRpyc = readGameFile("options.rpyc");
    // by standard this shouldnt be none, but if will be here just in case my ported logic has a flaw idk
    if (optionsRpyc.is_none()) {
      throw std::runtime_error("Could not find options.rpyc in mod or base game.");
    }

    auto statements = ModsIndex::rpycReaderModule().attr("get_rpyc_statements")(ModsIndex::rpycReaderModule().attr("peek_rpyc")(optionsRpyc)).cast<py::list>();

    /*
    std::unordered_map<std::string, std::vector<std::string>> requestedDefines {
      {"config", { "name", "window_icon", "version", "save_directory" }},
      {"build",  {"name"}}
    };
    */

    // this code is responsible for banning msvc from being used to build the app.
#define PYDICT_INIT(...) ({\
      py::dict ret;\
      __VA_ARGS__\
      ret;\
    })
#define PYDICT_ARG(key,value) ret[key] = value;
#define PYLIST_INIT__() PYLIST_INIT_
#define PYLIST_INIT_(i, ...) \
    ret2.append(i);\
    __VA_OPT__(DEFER1(PYLIST_INIT__)()(__VA_ARGS__))
#define PYLIST_INIT(...) ({\
      py::list ret2;\
      EVAL(PYLIST_INIT_(__VA_ARGS__))\
      ret2;\
    })

    py::dict requestedDefines = PYDICT_INIT(
      PYDICT_ARG("config", PYLIST_INIT("name", "window_icon", "version", "save_directory"))
      PYDICT_ARG("build", PYLIST_INIT("name"))
    );

    auto defines = ModsIndex::rpycReaderModule().attr("lookup_defines")(statements, requestedDefines).cast<py::dict>();

    std::string name = defines.contains("config.name") ? defines["config.name"].cast<std::string>() : "Doki Doki Modding Club!";
    std::string version = defines.contains("config.version") ? defines["config.version"].cast<std::string>() : "1.0.0";
    std::string icon = defines.contains("config.window_icon") ? defines["config.window_icon"].cast<py::str>().attr("removeprefix")("/").cast<std::string>() : "";
    std::string buildId = defines.contains("build.name") ? defines["build.name"].cast<std::string>() : "DDLC";
    std::string saveDirectory = defines.contains("config.save_directory") ? defines["config.save_directory"].cast<std::string>() : "DDLC";

    std::string id = generate_uuid_v4();

    auto iconContentPy = readGameFile(icon);
    if (iconContentPy.is_none()) {
      // im just too tired
      throw std::runtime_error("Create an issue on GitHub to tell me to implement the edge case of icon content being null.");
    }

    // turn a python bytes to char*
    std::string_view sv = iconContentPy.cast<py::bytes>(); // ?
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(sv.data());
    ModsIndex::writeIcons(id, const_cast<unsigned char*>(ptr), sv.size());


    Mod mod {
      id,
      name,
      version,
      buildId,
      path.string(),
      false
    };
    modsList.push_back(mod);

    saveModsIndex();
  }
}
