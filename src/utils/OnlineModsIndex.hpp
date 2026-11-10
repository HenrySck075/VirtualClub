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

/// Subclass for Doki Doki Modding Central
///
/// also site so slopped that they send all 600 mod entries in one single json lmao
class ModdingCentralIndex : public OnlineIndex {
private:
  bool m_fetched = false;
  /// Everything. 
  ///
  /// Initially m_modsList[ListSorting::Oldest] will be populated (order sent by the server), 
  /// then requested sorting types will be built based on this
  std::unordered_map<ListSorting, std::vector<OnlineMod>> m_modsList;

  void createSortedListIfAbsent(ListSorting sorting);
  std::vector<OnlineMod> selectList(QueryConfig config);
public:
  QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) override;
  int getTotalMods() override;

  QString displayName() override {
    return "Doki Doki Modding Central";
  }

  explicit ModdingCentralIndex(QObject* parent = nullptr) : OnlineIndex(parent) {}
};

/// Subclass for dokimods.me
///
/// This one has support for server-side perPage limit but the site sets ?perPage=500 
/// so it basically sends everything anyway and does pagination client-side
class DokiModsDotMeIndex : public OnlineIndex {
  int m_total = 0;
public:
  QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) override;
  int getTotalMods() override;

  QString displayName() override {
    return "dokimods.me";
  }

  explicit DokiModsDotMeIndex(QObject* parent = nullptr) : OnlineIndex(parent) {}
};
