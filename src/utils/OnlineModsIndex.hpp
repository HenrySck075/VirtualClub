#pragma once

#include <vector>
#include <QFuture>
#include <QString>

struct OnlineMod {
  QString name;
  QString description;
  QString author;
  QString version;

  QString imageUrl;
  QString url;
};

enum class ListSorting {
  Newest, Oldest, TitleAZ, TitleZA, Author
};
/// Abstract class to get information about a mod on an online mods index
class OnlineIndex : public QObject {
public:
  struct QueryConfig {
    int page = 0;
    int perPage = 18;
    ListSorting sorting = ListSorting::Newest;
  };
  virtual QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) = 0;
};

///////////////////////////////////////

/// Subclass for Doki Doki Modding Central
///
/// also site so slopped that they send all 600 mod entries in one single json lmao
class ModdingCentralIndex : public OnlineIndex {
  bool m_fetched = false;
  /// Everything. 
  ///
  /// Initially m_modsList[ListSorting::Oldest] will be populated (order sent by the server), 
  /// then requested sorting types will be built based on this when requested
  std::unordered_map<ListSorting, std::vector<OnlineMod>> m_modsList;
public:
  QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) override;
};

/// Subclass for dokimods.me
///
/// This one has support for server-side perPage limit but the site sets ?perPage=500 
/// so it basically sends everything anyway and does pagination client-side
class DokiModsDotMeIndex : public OnlineIndex {
public:
  QFuture<std::vector<OnlineMod>> getMods(QueryConfig config) override;
};
