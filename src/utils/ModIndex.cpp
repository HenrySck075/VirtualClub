#include "ModIndex.hpp"
#include "consts.hpp"
#include "utils/ProfileSettings.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <QStandardPaths>
#include <stdexcept>
#include <utility>
#include "RenpyArchive.hpp"
#include "macros.h"
#include "utils/ProfileSingleApp.hpp"
#include "utils/utils.hpp"
#include <QDebug>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "../third_party/stb/stb_image_resize2.h"


ModsIndex::ModsIndex(const QString &path)
: QObject(nullptr), m_store(std::make_shared<YamlSettings>(path)) {
  qRegisterMetaType<ModsIndex::Mod>();
  qDebug() << "ModsIndex: initializing store at" << path;
  m_modsList.clear();

  auto mods = m_store->value("mods").toMap();
  qDebug() << "ModsIndex: loaded" << mods.size() << "stored mods";

  for (const auto& modVariant : mods.asKeyValueRange()) {
    qDebug() << "ModsIndex: deserializing mod" << modVariant.first;
    auto modMap = modVariant.second.toMap();
    Mod mod {
      modVariant.first.toStdString(),
      modMap["name"].toString().toStdString(),
      modMap["version"].toString().toStdString(),
      modMap["buildId"].toString().toStdString(),
      modMap["path"].toString().toStdString(),
      modMap["enableDeveloper"].toBool(),
      modMap["forceRecompile"].toBool()
    };
    m_modsList.push_back(mod);
  }
  qDebug() << "ModsIndex: initialization complete; cached" << m_modsList.size() << "mods";
}

std::shared_ptr<ModsIndex> ModsIndex::get() {
  qDebug() << "ModsIndex::get: resolving singleton";
  static auto newInstance =
     std::shared_ptr<ModsIndex>(new ModsIndex(QString("profiles/mods/%1.yaml").arg(
        ProfileSingleApp::instance()->profileId()
      )));
  qDebug() << "ModsIndex::get: singleton ready for profile"
           << ProfileSingleApp::instance()->profileId();
  return newInstance;
}

const std::vector<ModsIndex::Mod>& ModsIndex::getMods() {
  qDebug() << "ModsIndex::getMods: returning" << m_modsList.size() << "mods";
  return m_modsList;
}

const ModsIndex::Mod& ModsIndex::getModByID(std::string id) {
  qDebug() << "ModsIndex::getModByID: looking up" << QString::fromStdString(id);
  auto it = std::find_if(m_modsList.begin(), m_modsList.end(), [&id](const Mod& mod) {
    return mod.id == id;
  });

  if (it != m_modsList.end()) {
    qDebug() << "ModsIndex::getModByID: found" << QString::fromStdString(it->name)
             << "at" << QString::fromStdString(it->modPath);
    return *it;
  } else {
    qWarning() << "ModsIndex::getModByID: mod not found:" << QString::fromStdString(id);
    throw std::runtime_error("Mod with ID " + id + " not found.");
  }
}


ModsIndex::~ModsIndex() { 
  qDebug() << "ModsIndex: saving" << m_modsList.size() << "mods";
  QMap<QString, QVariant> mods;
  for (const auto& mod : m_modsList) {
    QVariantMap modMap;
    
    modMap["name"] = QString::fromStdString(mod.name);
    modMap["version"] = QString::fromStdString(mod.version);
    modMap["buildId"] = QString::fromStdString(mod.buildId);
    modMap["path"] = QString::fromStdString(mod.modPath);
    modMap["enableDeveloper"] = mod.enableDeveloper;
    modMap["forceRecompile"] = mod.forceRecompile;
    mods[QString::fromStdString(mod.id)] = modMap;
  }

  m_store->setValue("mods", mods);
  qDebug() << "ModsIndex: destructor store update complete";
}

void writeIcons(std::string id, unsigned char* buffer, int len) {
  qDebug() << "ModsIndex: decoding icon for mod" << QString::fromStdString(id)
           << "size" << len;
  int x,y,comp;
  unsigned char* content = stbi_load_from_memory(buffer, len, &x, &y, &comp, 0);
  
  if (!content) {
    qWarning() << "ModsIndex: failed to decode icon for mod" << QString::fromStdString(id)
               << ":" << stbi_failure_reason();
    throw std::invalid_argument(std::string("Failed to decode image: ") + stbi_failure_reason());
  }
  qDebug() << "ModsIndex: decoded icon" << x << "x" << y << "channels" << comp;

  static auto iconsDir = std::filesystem::path(
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
  ) / "icons";

  // save the (.png'd) original under {id}.png
  // and a 80x80 downscaled version under {id}.scaled.png

  if (!std::filesystem::exists(iconsDir)) {
    qDebug() << "ModsIndex: creating icon directory" << QString::fromStdString(iconsDir.string());
    std::error_code ec;
    std::filesystem::create_directories(iconsDir, ec);
    if (ec) {
      qWarning() << "ModsIndex: could not create icon directory:" << QString::fromStdString(ec.message());
      throw std::filesystem::filesystem_error("Could not create icon directory", iconsDir, ec);
    }
  }

  std::filesystem::path originalPath = iconsDir / (id + ".png");
  std::filesystem::path scaledPath = iconsDir / (id + ".scaled.png");

  qDebug() << "ModsIndex: writing original icon" << QString::fromStdString(originalPath.string());
  if (!stbi_write_png(originalPath.string().c_str(), x, y, comp, content, x * comp)) {
    qWarning() << "ModsIndex: failed to write original icon";
  }
  unsigned char* scaledContent = new unsigned char[80 * 80 * comp];
  stbir_resize_uint8_linear(content, x, y, 0, scaledContent, 80, 80, 0, (stbir_pixel_layout)(comp));
  qDebug() << "ModsIndex: writing scaled icon" << QString::fromStdString(scaledPath.string());
  if (!stbi_write_png(scaledPath.string().c_str(), 80, 80, comp, scaledContent, 80 * comp)) {
    qWarning() << "ModsIndex: failed to write scaled icon";
  }

  stbi_image_free(content);
  delete[] scaledContent;
}
void writeIconsFromFile(std::string id, std::string file) {
  qDebug() << "ModsIndex: loading icon from file" << QString::fromStdString(file);
  std::ifstream inputFile(file, std::ios::binary);
  if (!inputFile) {
      qWarning() << "ModsIndex: could not open icon file" << QString::fromStdString(file);
      throw std::runtime_error("Could not open file: " + file);
  }
  auto fileSize = std::filesystem::file_size(file);
  qDebug() << "ModsIndex: icon file size" << static_cast<quint64>(fileSize);
  std::vector<unsigned char> buffer(fileSize);
  inputFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
  writeIcons(id, buffer.data(), fileSize);
}

void ModsIndex::updateMod(const Mod& mod, const QString& iconPath) {
  auto it = std::find_if(m_modsList.begin(), m_modsList.end(),
                         [&mod](const Mod& candidate) {
                           return candidate.id == mod.id;
                         });
  if (it == m_modsList.end()) {
    throw std::invalid_argument("Supplied mod of ID " + mod.id + " does not exist.");
  }

  *it = mod;

  if (!iconPath.isEmpty()) {
    QPixmap source(iconPath);
    if (source.isNull()) {
      throw std::invalid_argument("Could not load the selected icon image.");
    }

    QSize squareSize(std::max(source.width(), source.height()),
                     std::max(source.width(), source.height()));
    QPixmap square = source.scaled(squareSize, Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);
    QRect crop((square.width() - std::min(source.width(), source.height())) / 2,
               (square.height() - std::min(source.width(), source.height())) / 2,
               std::min(source.width(), source.height()),
               std::min(source.width(), source.height()));
    square = square.copy(crop);
    square = square.scaled(512, 512, Qt::IgnoreAspectRatio,
                           Qt::SmoothTransformation);

    const auto iconsDir = std::filesystem::path(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
    ) / "icons";
    std::error_code ec;
    std::filesystem::create_directories(iconsDir, ec);
    if (ec) {
      throw std::filesystem::filesystem_error("Could not create icon directory", iconsDir, ec);
    }

    const auto originalPath = iconsDir / (mod.id + ".png");
    const auto scaledPath = iconsDir / (mod.id + ".scaled.png");
    if (!square.save(QString::fromStdString(originalPath.string()), "PNG")) {
      throw std::runtime_error("Could not write the mod icon.");
    }
    const QPixmap scaled = square.scaled(80, 80, Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);
    if (!scaled.save(QString::fromStdString(scaledPath.string()), "PNG")) {
      throw std::runtime_error("Could not write the scaled mod icon.");
    }
  }

  QMap<QString, QVariant> mods;
  for (const auto& storedMod : m_modsList) {
    QVariantMap modMap;
    modMap["name"] = QString::fromStdString(storedMod.name);
    modMap["version"] = QString::fromStdString(storedMod.version);
    modMap["buildId"] = QString::fromStdString(storedMod.buildId);
    modMap["path"] = QString::fromStdString(storedMod.modPath);
    modMap["enableDeveloper"] = storedMod.enableDeveloper;
    modMap["forceRecompile"] = storedMod.forceRecompile;
    mods[QString::fromStdString(storedMod.id)] = modMap;
  }
  m_store->setValue("mods", mods);
  m_store->save();
  emit modUpdated(mod);
}

std::string ModsIndex::Mod::getIconPath() const {
  qDebug() << "ModsIndex: resolving icon path for mod" << QString::fromStdString(id);
  const auto iconsDir = std::filesystem::path(
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
  ) / "icons";
  return (iconsDir / (id + ".png")).string();
};

ModsIndex::Mod ModsIndex::installMod(std::filesystem::path path) {
  qDebug() << "ModsIndex::installMod: starting installation for"
           << QString::fromStdString(path.string());
  // installMod() may be invoked by a QThreadPool worker. pybind11 objects
  // require an active GIL whenever they are created or destroyed.
  py::gil_scoped_acquire gil;

  auto settings = ProfileSettings::get();
  auto modGameDir = path / "game";
  auto baseGameDir = std::filesystem::path(settings->value(STK_BASEPATH).toString().toStdString()) / "game";
  qDebug() << "ModsIndex::installMod: mod game dir"
           << QString::fromStdString(modGameDir.string())
           << "base game dir" << QString::fromStdString(baseGameDir.string());

  // TODO: Analyze EVERY files
  auto readGameFile = [&modGameDir, &baseGameDir](const std::string& filepath) {
    return ArchiveReader::readGameFile({modGameDir, baseGameDir}, filepath);
  };
  auto optionsRpyc = readGameFile("options.rpyc");
  // by standard this shouldnt be none, but if will be here just in case my ported logic has a flaw idk
  if (optionsRpyc.is_none()) {
    qWarning() << "ModsIndex::installMod: options.rpyc not found";
    throw std::runtime_error("Could not find options.rpyc in mod or base game.");
  }
  qDebug() << "ModsIndex::installMod: options.rpyc resolved";

  auto rpycReaderModule = ArchiveReader::rpycReaderModule();
  auto statements = rpycReaderModule.attr("get_rpyc_statements")(rpycReaderModule.attr("peek_rpyc")(optionsRpyc)).cast<py::list>();

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

  auto defines = rpycReaderModule.attr("lookup_defines")(statements, requestedDefines).cast<py::dict>();

  std::string name = defines.contains("config.name") ? defines["config.name"].cast<std::string>() : "Doki Doki Modding Club!";
  std::string version = defines.contains("config.version") ? defines["config.version"].cast<std::string>() : "1.0.0";
  std::string icon = defines.contains("config.window_icon") ? defines["config.window_icon"].cast<py::str>().attr("removeprefix")("/").cast<std::string>() : "";
  std::string buildId = defines.contains("build.name") ? defines["build.name"].cast<std::string>() : "DDLC";
  std::string saveDirectory = defines.contains("config.save_directory") ? defines["config.save_directory"].cast<std::string>() : "DDLC";
  qDebug() << "ModsIndex::installMod: metadata name=" << QString::fromStdString(name)
           << "version=" << QString::fromStdString(version)
           << "buildId=" << QString::fromStdString(buildId)
           << "saveDirectory=" << QString::fromStdString(saveDirectory)
           << "icon=" << QString::fromStdString(icon);

  std::string id = generate_uuid_v4();
  qDebug() << "ModsIndex::installMod: generated mod id" << QString::fromStdString(id);

  auto iconContentPy = readGameFile(icon);
  if (iconContentPy.is_none()) {
    qWarning() << "ModsIndex::installMod: icon content is null for mod" << QString::fromStdString(name);
    // im just too tired
    throw std::runtime_error("Create an issue on GitHub to tell me to implement the edge case of icon content being null.");
  }

  // turn a python bytes to char*
  std::string_view sv = iconContentPy.cast<py::bytes>(); // ?
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(sv.data());
  writeIcons(id, const_cast<unsigned char*>(ptr), sv.size());


  ModsIndex::Mod mod {
    id,
    name,
    version,
    buildId,
    path.string(),
    false
  };
  m_modsList.push_back(mod);
  qDebug() << "ModsIndex::installMod: installed" << QString::fromStdString(name)
           << "id" << QString::fromStdString(id)
           << "total mods" << m_modsList.size();

  emit modInstalled(mod);
  return mod;
}
void ModsIndex::removeMod(Mod& mod) {
  qDebug() << "ModsIndex::removeMod: removing" << QString::fromStdString(mod.id)
           << QString::fromStdString(mod.name);
  const auto modId = mod.id;
  auto i = std::find(m_modsList.begin(), m_modsList.end(), mod);
  if (i == m_modsList.end()) {
    qWarning() << "ModsIndex::removeMod: mod not found:" << QString::fromStdString(mod.id);
    throw std::invalid_argument("Supplied mod of ID " + mod.id + " does not exist.");
  }
  m_modsList.erase(i);

  // delete the icons
  auto iconsDir = std::filesystem::path(
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()
  ) / "icons";
  auto originalIcon = iconsDir / (modId + ".png");
  auto scaledIcon = iconsDir / (modId + ".scaled.png");
  std::error_code ec;
  qDebug() << "ModsIndex::removeMod: deleting icon files" << QString::fromStdString(originalIcon.string())
           << QString::fromStdString(scaledIcon.string());
  std::filesystem::remove(originalIcon, ec);
  if (ec) qWarning() << "ModsIndex::removeMod: could not remove original icon:" << QString::fromStdString(ec.message());
  ec.clear();
  std::filesystem::remove(scaledIcon, ec);
  if (ec) qWarning() << "ModsIndex::removeMod: could not remove scaled icon:" << QString::fromStdString(ec.message());
  qDebug() << "ModsIndex::removeMod: complete; remaining mods" << m_modsList.size();

  emit modRemoved(QString::fromStdString(modId));
}


void ModsIndex::addModToRecentlyPlayed(const std::string& id) {
  qDebug() << "ModsIndex::recent: adding mod" << QString::fromStdString(id);
  auto qid = QString::fromStdString(id);
  auto list = m_store->value(STK_RECENTLIST, QVariantList()).toList();

  if (list.contains(qid)) {
    list.removeAll(qid);
  }
  if (list.size() >= 10) {
    list.removeLast();
  }
  list.prepend(qid);
  m_store->setValue(STK_RECENTLIST, list);
  qDebug() << "ModsIndex::recent: updated list size" << list.size();
  emit recentlyPlayedUpdated();
}

const std::vector<ModsIndex::Mod>& ModsIndex::getRecentlyPlayed() {
  qDebug() << "ModsIndex::recent: resolving recently played mods";
  static std::vector<Mod> recentlyPlayed;
  recentlyPlayed.clear();

  auto list = m_store->value(STK_RECENTLIST, QVariantList()).toList();
  qDebug() << "ModsIndex::recent: stored entries" << list.size();

  for (const auto& idVariant : list) {
    QString id = idVariant.toString();
    auto it = std::find_if(m_modsList.begin(), m_modsList.end(), [&id](const Mod& mod) {
      return QString::fromStdString(mod.id) == id;
    });
    if (it != m_modsList.end()) {
      recentlyPlayed.push_back(*it);
    } else {
      qWarning() << "ModsIndex::recent: skipping missing mod" << id;
    }
  }
  qDebug() << "ModsIndex::recent: resolved entries" << recentlyPlayed.size();

  return recentlyPlayed;
}

std::optional<ModsIndex::Mod::PlaytimePair> ModsIndex::Mod::getPlaytime() const {
  const QString qSavedir = QString("%1/saves/%2")
                               .arg(QStandardPaths::writableLocation(
                                        QStandardPaths::AppDataLocation),
                                    this->id);

  const std::filesystem::path filePath =
      std::filesystem::path(qSavedir.toStdString()) / ".mvc_playtime";
  qDebug() << "ModsIndex::playtime: reading" << QString::fromStdString(filePath.string())
           << "for mod" << QString::fromStdString(id);

  std::ifstream file(filePath);
  if (!file.is_open()) {
    qDebug() << "ModsIndex::playtime: no playtime file for mod" << QString::fromStdString(id);
    return std::nullopt;
  }

  long long rawSeconds = 0;
  long long activeSeconds = 0;

  if (file >> rawSeconds >> activeSeconds) {
    qDebug() << "ModsIndex::playtime: mod" << QString::fromStdString(id)
             << "raw seconds" << rawSeconds << "active seconds" << activeSeconds;
    return std::make_pair(std::chrono::seconds(rawSeconds),
                          std::chrono::seconds(activeSeconds));
  }

  qWarning() << "ModsIndex::playtime: malformed playtime file" << QString::fromStdString(filePath.string());
  return std::nullopt;
}
