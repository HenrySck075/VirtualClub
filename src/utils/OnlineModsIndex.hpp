#pragma once

#include <vector>
#include <QFuture>
#include <QString>

struct OnlineMod {
  std::string name;
  std::string description;
  std::string author;
  std::string version;

  std::string imageUrl;
  std::vector<std::string> screenshotImageUrls;
  std::string url;
};

enum class ListSorting {
  Newest, Oldest, TitleAZ, TitleZA, Author
};
class QNetworkAccessManager;
/// Abstract class to get information about a mod on an online mods index
class OnlineIndex : public QObject {
public:
  struct QueryConfig {
    int page = 0;
    int perPage = 18;
    ListSorting sorting = ListSorting::Newest;
  };
  virtual QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) = 0;
  virtual int getTotalMods() = 0;

  virtual QString displayName() = 0;

  explicit OnlineIndex(QObject* parent = nullptr) : QObject(parent) {}
};

///////////////////////////////////////
/// Subclass for dokimods.me
///
/// This one has support for server-side perPage limit but the site sets ?perPage=500 
/// so it basically sends everything anyway and does pagination client-side
///
/// Update: the reason was because the list was sent in TitleZA order... split into several "chunks". or in other words, semi-arbitrary.
/// This endpoint also DOES NOT CACHE. whrer should i report this?
class DokiModsDotMeIndex : public OnlineIndex {
  bool m_fetched = false;
  QNetworkAccessManager* m_nm = nullptr;
  std::unordered_map<ListSorting, std::vector<OnlineMod>> m_modsList;
  void createSortedListIfAbsent(ListSorting sorting);
  std::vector<OnlineMod> selectList(QueryConfig config);
public:
  QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) override;
  int getTotalMods() override;

  QString displayName() override {
    return "dokimods.me";
  }

  explicit DokiModsDotMeIndex(QObject* parent = nullptr);
};

///////////////////////////////////////
/// Subclass for Elysium Team (upcoming)
class ElysiumIndex : public OnlineIndex {};
