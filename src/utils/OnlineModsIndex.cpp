#include "OnlineModsIndex.hpp"

#include "utils/utils.hpp"
#include "utils/macros.h"

#ifdef MVC_DEBUG
#include <cpptrace/from_current_macros.hpp>
#include <cpptrace/from_current.hpp>
#endif

///////////////

DokiModsDotMeIndex::DokiModsDotMeIndex(QObject *parent)
  : OnlineIndex(parent) {
    m_nm = createNetworkAccessManager(this);
  }

QFuture<std::vector<OnlineMod>> DokiModsDotMeIndex::getMods(OnlineIndex::QueryConfig config) {
  if (m_modsList.empty()) {
    return fetchJsonAsync(m_nm, QUrl("https://dokimods.me/api/mods?perPage=500")).then([this, config](nlohmann::json resp) {
      assert(resp.is_object());

      auto& mlj = resp["data"];
      assert(mlj.is_array());
      
      CPPTRACE_TRY {
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
          list.push_back({
            .name = item["title"],
            .description = item["description"],
            .author = authors,
            .version = "",
            .imageUrl = "https://dokimods.me/pocketbase//api/files/"
                        +item["collectionId"].get<std::string>()
                        +"/"
                        +item["id"].get<std::string>()
                        +"/"
                        +item["thumbnail"].get<std::string>(),
            .url = item["download_link"]
          });
        }

        m_modsList[ListSorting::Oldest] = list;

        createSortedListIfAbsent(config.sorting);

        return selectList(config);
      } CPPTRACE_CATCH (...) {
        MVC_DEBUG_EVAL(cpptrace::from_current_exception().print());
        throw;
      }
    });
  } else {
    createSortedListIfAbsent(config.sorting);
    return QtFuture::makeReadyValueFuture(selectList(config));
  }
}
int DokiModsDotMeIndex::getTotalMods() {
  if (m_modsList.contains(ListSorting::Oldest)) {
    return m_modsList[ListSorting::Oldest].size();
  }
  return 0;
}
std::vector<OnlineMod> DokiModsDotMeIndex::selectList(QueryConfig config) {
  auto sort = config.sorting;

  auto& list = m_modsList[sort];

  int startIndex = config.page * config.perPage;
  int endIndex = std::min(startIndex + config.perPage, static_cast<int>(list.size()));

  if (startIndex >= static_cast<int>(list.size())) {
    return {};
  }

  return std::vector<OnlineMod>(list.begin() + startIndex, list.begin() + endIndex);
}
void DokiModsDotMeIndex::createSortedListIfAbsent(ListSorting sort) {
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
