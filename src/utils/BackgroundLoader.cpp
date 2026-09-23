#include "BackgroundLoader.hpp"
#include <QTime>
#include <QImage>
#include <QCoreApplication>
#include <filesystem>
#include "utils/ProfileSettings.hpp"

// Helper to parse times like "18", "18.30", "6.15", "6" into QTime
QTime parseFlexibleTime(const std::string& str) {
    QString qstr = QString::fromStdString(str).trimmed();
    QTime t = QTime::fromString(qstr, "H.m");
    if (!t.isValid()) {
        t = QTime::fromString(qstr, "H");
    }
    return t;
}

// Checks if current time falls within [start, end], handling midnight wraps
bool isTimeInRange(const QTime& current, const QTime& start, const QTime& end) {
    if (start <= end) {
        // Standard range within the same day (e.g., 08:00 to 16:30)
        return current >= start && current <= end;
    } else {
        // Overnight range wrapping past midnight (e.g., 18:00 to 06:00)
        return current >= start || current <= end;
    }
}

struct BgImageCandidate {
    QTime startTime;
    QTime endTime;
    std::filesystem::path path;
};

namespace fs = std::filesystem;
inline fs::path getBGAssetsPath() {
  return fs::path(QCoreApplication::applicationDirPath().toStdString()) / "assets" / "bg";
}


inline fs::path getBGPackPath(const QString& pack) {
  auto p = getBGAssetsPath() / pack.toStdString();

  if (!fs::exists(p)) {
    throw nonexistent_path{};
  }

  return p;
}

QStringList BackgroundLoader::getPacks() {
  // literally just list every folders under getBGAssetsPath
  // i mean i could just implement some checks but idc atm

  QStringList packs;
  for (const auto& entry : fs::directory_iterator(getBGAssetsPath())) {
      if (entry.is_directory()) {
          packs.append(QString::fromStdString(entry.path().filename().string()));
      }
  }

  return packs;
}

QString BackgroundLoader::getImage(const QString& pack_) {
  auto pack = pack_.isEmpty() ? ProfileSettings::get()->value("background").toString() : pack_;

  fs::path bgAssetsFolder;
  try {
    bgAssetsFolder = getBGPackPath(pack);
  } catch (const nonexistent_path& e) {
    return ""; // Return null/empty QImage if path does not exist
  }

  std::vector<BgImageCandidate> candidates;

  // Collect and parse files
  for (const auto& entry : fs::directory_iterator(bgAssetsFolder)) {
      if (!entry.is_regular_file()) continue;

      std::string filename = entry.path().stem().string(); // filename without extension
      size_t dashIdx = filename.find('-');
      if (dashIdx == std::string::npos) continue;

      std::string startStr = filename.substr(0, dashIdx);
      std::string endStr = filename.substr(dashIdx + 1);

      QTime startTime = parseFlexibleTime(startStr);
      QTime endTime = parseFlexibleTime(endStr);

      if (startTime.isValid() && endTime.isValid()) {
          candidates.push_back({startTime, endTime, entry.path()});
      }
  }

  // Sort in reverse numerical order based on start time (e.g., 20:00 before 18:00 before 06:00)
  std::sort(candidates.begin(), candidates.end(), [](const BgImageCandidate& a, const BgImageCandidate& b) {
      return a.startTime > b.startTime;
  });

  QTime currentTime = QTime::currentTime();

  // Iterate through candidates (now sorted in reverse numerical order)
  for (const auto& item : candidates) {
      if (isTimeInRange(currentTime, item.startTime, item.endTime)) {
          return QString::fromStdString(item.path.string());
      }
  }

  return ""; // Return null/empty QImage if no matching interval matches
}


