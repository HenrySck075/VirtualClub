#pragma once
#include "utils/FlowLayout.hpp"
#include "utils/OnlineModsIndex.hpp"
#include <QWidget>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QLabel>

class Router;

class OnlineModItem;

class OnlineModsIndexScreen : public QWidget {
  Q_OBJECT
private:
  OnlineIndex* m_currentIndex = nullptr;
  Router* m_router = nullptr;
  QNetworkAccessManager* m_nm = nullptr;

  FlowLayout* m_contentLayout = nullptr;
  QStackedWidget* m_container = nullptr;
  QScrollArea* m_scrollArea = nullptr;

  QWidget* m_content = nullptr;
  QWidget* m_placeholderWidget = nullptr;
  QWidget* m_exceptionWidget = nullptr;
  QLabel* m_exceptionLabel = nullptr;

  QWidget* m_footer = nullptr;

  int m_perPage = 18;
  int m_page = 0;
  int m_totalItems = 0;
  int m_totalPages = 1;
  ListSorting m_sort = ListSorting::Newest;
  std::string m_searchQuery;

  bool m_firstShown = false;

  std::vector<OnlineModItem*> m_whar;

public:
  explicit OnlineModsIndexScreen(Router* router, QWidget* parent = nullptr);

  void showEvent(QShowEvent* event) override;
signals:
  void totalItemsUpdated();
  void pageNumberUpdated();
  void perPageNumberUpdated();
protected:
  void queryIndex();

  void setTotalItems(int value);
  void setPage(int page);
  void setPerPage(int value);
};
