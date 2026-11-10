#include "OnlineModsIndex.hpp"

#include "utils/utils.hpp"


///////////////////////////////////

void ModdingCentralIndex::createSortedListIfAbsent(ListSorting sort) {
  if (m_modsList.contains(sort)) return;

  auto& base = m_modsList[ListSorting::Oldest];

  auto sorted = base;
  switch (sort) {
    case ListSorting::Newest: {
      std::reverse(sorted.begin(), sorted.end());
      m_modsList[sort] = sorted;
      break;
    }
    case ListSorting::TitleAZ: {
      std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
        return a.name < b.name;
      });
      m_modsList[sort] = sorted;
      break;
    }
    case ListSorting::TitleZA: {
      std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
        return a.name > b.name;
      });
      m_modsList[sort] = sorted;
      break;
    }
    case ListSorting::Author: {
      std::sort(sorted.begin(), sorted.end(), [](const OnlineMod& a, const OnlineMod& b) {
        return a.author < b.author;
      });
      m_modsList[sort] = sorted;
      break;
    }
    default:
      break; 
  }
}


std::vector<OnlineMod> ModdingCentralIndex::selectList(QueryConfig config) {
  auto sort = config.sorting;

  auto& list = m_modsList[sort];

  int startIndex = config.page * config.perPage;
  int endIndex = std::min(startIndex + config.perPage, static_cast<int>(list.size()));

  if (startIndex >= static_cast<int>(list.size())) {
    return {};
  }

  return std::vector<OnlineMod>(list.begin() + startIndex, list.begin() + endIndex);
}

QFuture<std::vector<OnlineMod>> ModdingCentralIndex::getMods(OnlineIndex::QueryConfig config) {
  auto sort = config.sorting;
  if (m_modsList.empty()) {
    auto* manager = createNetworkAccessManager(this);
    return fetchJsonAsync(manager, QUrl("https://ddmc.site/data/mods.json")).then([manager, sort, config, this](nlohmann::json resp) {

      auto mlj = resp["standard"];

      assert(mlj.is_array());

      std::vector<OnlineMod> list; // Oldest sorted
      list.reserve(mlj.size());

      for (auto& item : mlj) {
        list.push_back({
          .name = item["title"],
          .description = item["description"],
          .author = item["author"],
          .version = item["links"][0]["version"],
          .imageUrl = item["imageUrl"].get<std::string>().empty() ? "" : "https://ddmc.site"+item["imageUrl"].get<std::string>(),
          .url = item["links"][0]["url"]
        });
      }

      m_modsList[ListSorting::Oldest] = list;

      createSortedListIfAbsent(sort);

      manager->deleteLater();
      return selectList(config);
    });
  } else {
    createSortedListIfAbsent(sort);
    return QtFuture::makeReadyValueFuture(selectList(config));
  }
}
int ModdingCentralIndex::getTotalMods() {
  if (m_modsList.contains(ListSorting::Oldest)) {
    return m_modsList[ListSorting::Oldest].size();
  }
  return 0;
}



///////////////

QFuture<std::vector<OnlineMod>> DokiModsDotMeIndex::getMods(OnlineIndex::QueryConfig config) {
  return {};
}
int DokiModsDotMeIndex::getTotalMods() {
  return 0;
}
