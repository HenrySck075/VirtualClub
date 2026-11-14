#pragma once

#include <chrono>
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

  std::optional<std::chrono::sys_days> creationDate;
  std::optional<std::chrono::sys_days> lastUpdated;
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

    std::string searchQuery;
  };
  virtual QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) = 0;
  virtual int getTotalMods(QueryConfig config) = 0;

  virtual QString displayName() = 0;

  explicit OnlineIndex(QObject* parent = nullptr) : QObject(parent) {}
};

/// Subclass for classes getting the entire index at once and does every operation based on that data.
class FullFetchIndex : public OnlineIndex {
  bool m_fetched = false;
  QNetworkAccessManager* m_nm = nullptr;
  std::unordered_map<ListSorting, std::vector<OnlineMod>> m_modsList;
  void createSortedListIfAbsent(ListSorting sorting);
  std::vector<OnlineMod> filterList(
    const std::vector<OnlineMod>& list,
    const std::string& searchQuery
  ) const;
  std::vector<OnlineMod> selectList(QueryConfig config);
protected:
  QNetworkAccessManager* networkManager() const {return m_nm;}
public:
  virtual ListSorting baseSort() {return ListSorting::Oldest;}
  QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) override;
  int getTotalMods(QueryConfig config) override;

  virtual QFuture<std::vector<OnlineMod>> fetchMods() = 0;

  explicit FullFetchIndex(QObject* parent = nullptr);
};

///////////////////////////////////////
/// Subclass for dokimods.me
///
/// This one has support for server-side perPage limit but the site sets ?perPage=500 
/// so it basically sends everything anyway and does pagination client-side
///
/// Update: the reason was because the list was sent in TitleZA order... split into several "chunks". or in other words, semi-arbitrary.
/// This endpoint also DOES NOT CACHE. whrer should i report this?
class DokiModsDotMeIndex : public FullFetchIndex {
public:
  QFuture<std::vector<OnlineMod>> fetchMods() override;
  QString displayName() override {return "dokimods.me";}

  explicit DokiModsDotMeIndex(QObject* parent = nullptr)
    : FullFetchIndex(parent) {};
};


///////////////////////////////////////
/// Subclass for DDMC Mods List 
///
/// sourced from Reddit and mods list spreadsheet snapshot generated as JSON
class DDMCIndex : public FullFetchIndex {
public:
  ListSorting baseSort() override {return ListSorting::TitleAZ;}
  QFuture<std::vector<OnlineMod>> fetchMods() override;
  
  QString displayName() override {return "VirtualClub (DDMC)";}

  explicit DDMCIndex(QObject* parent = nullptr)
    : FullFetchIndex(parent) {}
};

///////////////////////////////////////
/// Subclass for Elysium Team (upcoming)
class ElysiumIndex : public OnlineIndex {};
