#include "OnlineModsIndex.hpp"
#include <chrono>

#include "utils/utils.hpp"
#include "utils/macros.h"

#include <QRegularExpression>


#ifdef MVC_DEBUG
#include <cpptrace/from_current_macros.hpp>
#include <cpptrace/from_current.hpp>
#endif

///////////////

FullFetchIndex::FullFetchIndex(QObject *parent)
  : OnlineIndex(parent) {
    m_nm = createNetworkAccessManager(this);
  }

QFuture<std::vector<OnlineMod>> FullFetchIndex::getMods(OnlineIndex::QueryConfig config) {
  if (m_modsList.empty()) {
    return fetchMods().then([this, config](std::vector<OnlineMod> mods) {
      m_modsList[baseSort()] = std::move(mods);
      createSortedListIfAbsent(config.sorting);
      return selectList(config);
    });
  } else {
    createSortedListIfAbsent(config.sorting);
    return QtFuture::makeReadyValueFuture(selectList(config));
  }
}
int FullFetchIndex::getTotalMods(QueryConfig config) {
  if (!m_modsList.contains(baseSort())) {
    return 0;
  }

  createSortedListIfAbsent(config.sorting);
  return filterList(m_modsList[config.sorting], config.searchQuery).size();
}
std::vector<OnlineMod> FullFetchIndex::filterList(
  const std::vector<OnlineMod>& list,
  const std::string& searchQuery
) const {
  if (searchQuery.empty()) {
    return list;
  }

  const auto tokens = QString::fromStdString(searchQuery)
    .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  if (tokens.empty()) {
    return list;
  }

  struct Match {
    OnlineMod mod;
    int titleMatches;
  };
  std::vector<Match> matches;
  matches.reserve(list.size());

  for (const auto& mod : list) {
    const auto title = QString::fromStdString(mod.name);
    const auto author = QString::fromStdString(mod.author);
    const auto description = QString::fromStdString(mod.description);
    int titleMatches = 0;
    bool matchesAllTokens = true;

    for (const auto& token : tokens) {
      const bool titleMatch = title.contains(token, Qt::CaseInsensitive);
      const bool fieldMatch = titleMatch
        || author.contains(token, Qt::CaseInsensitive)
        || description.contains(token, Qt::CaseInsensitive);
      if (!fieldMatch) {
        matchesAllTokens = false;
        break;
      }
      titleMatches += titleMatch;
    }

    if (matchesAllTokens) {
      matches.push_back({mod, titleMatches});
    }
  }

  std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
    return a.titleMatches > b.titleMatches;
  });

  std::vector<OnlineMod> filtered;
  filtered.reserve(matches.size());
  for (auto& match : matches) {
    filtered.push_back(std::move(match.mod));
  }
  return filtered;
}
std::vector<OnlineMod> FullFetchIndex::selectList(QueryConfig config) {
  auto sort = config.sorting;

  auto list = filterList(m_modsList[sort], config.searchQuery);

  int startIndex = config.page * config.perPage;
  int endIndex = std::min(startIndex + config.perPage, static_cast<int>(list.size()));

  if (startIndex >= static_cast<int>(list.size())) {
    return {};
  }

  return std::vector<OnlineMod>(list.begin() + startIndex, list.begin() + endIndex);
}
void FullFetchIndex::createSortedListIfAbsent(ListSorting sort) {
  if (m_modsList.contains(sort)) return;

  auto baseSorting = baseSort();

  if (!m_modsList.contains(baseSorting)) {
    throw std::invalid_argument("Base sorting list is not available. Please run getMods() first.");
  }

  auto& base = m_modsList[baseSorting];

  auto sorted = base;
  bool isSorted = true;
  switch (sort) {
    case ListSorting::Oldest: {
      if (baseSorting == ListSorting::Newest) {
        std::reverse(sorted.begin(), sorted.end());
      } else {
        std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
          return a.lastUpdated < b.lastUpdated;
        });
      }
      
      break;
    }

    case ListSorting::Newest: {
      if (baseSorting == ListSorting::Oldest) {
        std::reverse(sorted.begin(), sorted.end());
      } else {
        std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
          return a.lastUpdated > b.lastUpdated;
        });
      }
      break;
    }
    case ListSorting::TitleAZ: {
      if (baseSorting == ListSorting::TitleZA) {
        std::reverse(sorted.begin(), sorted.end());
      } else {
        std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
          return a.name < b.name;
        });
      }
      break;
    }
    case ListSorting::TitleZA: {
      if (baseSorting == ListSorting::TitleAZ) {
        std::reverse(sorted.begin(), sorted.end());
      } else {
        std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
          return a.name > b.name;
        });
      }
      break;
    }
    case ListSorting::Author: {
      std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
        return a.author < b.author;
      });
      break;
    }
    default:
      isSorted = false;
      break; 
  }

  if (isSorted) 
    m_modsList[sort] = sorted;
}


//////////////////////

std::chrono::local_time<std::chrono::minutes> parseIso8601(std::string dt) {
  std::tm tm = {};
  std::istringstream ss(dt);
  
  // Parse using traditional C-style formatting flags via std::get_time
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  
  if (ss.fail()) {
    throw std::runtime_error("Failed to parse datetime string: " + dt);
  }
  auto tp = std::chrono::sys_days{std::chrono::year{tm.tm_year + 1900} / 
                                  (tm.tm_mon + 1) / tm.tm_mday} +
            std::chrono::hours{tm.tm_hour} + 
            std::chrono::minutes{tm.tm_min};
            
  std::chrono::local_time<std::chrono::minutes> local_min{tp.time_since_epoch()};

  return local_min;
}

QFuture<std::vector<OnlineMod>> DokiModsDotMeIndex::fetchMods() {
  return fetchJsonAsync(networkManager(), QUrl("https://dokimods.me/api/mods?perPage=500")).then([this](nlohmann::json resp) {
    assert(resp.is_object());

    auto& mlj = resp["data"];
    assert(mlj.is_array());
  
    // need to sort the list by oldest since its in arbitrary order
    // using the item["updated"] string, an iso 8601 formatted datetime
    std::sort(mlj.begin(), mlj.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
      return a["updated"].get<std::string>() < b["updated"].get<std::string>();
    });

    std::vector<OnlineMod> list;
    list.reserve(resp.size());

    for (auto& item : mlj) {
      std::string authors;
      auto& al = item["authors"];
      if (al.is_array()) {
        if (al.size() > 2) {
          authors = al[0].get<std::string>() + ", " + al[1].get<std::string>() + " et al.";
        } else if (al.size() == 2) {
          authors = al[0].get<std::string>() + ", " + al[1].get<std::string>();
        } else if (al.size() == 1) {
          authors = al[0].get<std::string>();
        }
      }
      std::string baseImageUrl = "https://dokimods.me/pocketbase//api/files/"
                    +item["collectionId"].get<std::string>()
                    +"/"
                    +item["id"].get<std::string>()
                    +"/";
      
      std::vector<std::string> screenshots;
      for (auto& s : item["screenshots"]) {
        screenshots.push_back(baseImageUrl + s.get<std::string>());
      }
      list.push_back({
        .name = item["title"],
        .description = item["description"],
        .author = authors,
        .version = "",
        .imageUrl = baseImageUrl
                    +item["thumbnail"].get<std::string>(),
        .screenshotImageUrls = screenshots,
        .url = item["download_link"],

        .creationDate = parseIso8601(item["created"].get<std::string>()),
        .lastUpdated = parseIso8601(item["updated"].get<std::string>())
      });
    }
    
    return list;
  });
}
