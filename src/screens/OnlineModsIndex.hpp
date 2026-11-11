#pragma once
#include "utils/FlowLayout.hpp"
#include "utils/OnlineModsIndex.hpp"
#include <QWidget>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>

class Router;

namespace detail {

class OMIThumbnail : public QWidget {
  Q_OBJECT
private:
  bool m_hovered = false;

  QNetworkAccessManager* m_nm;

  QWidget* m_child = nullptr;
  
  const int m_padding = 8;
  const int m_radius = 4;
public:
  void setHovered(bool hovered); 

  void resizeEvent(QResizeEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void updateChildRender();

  /*
   * @params displaySize image display size (including padding for the hover border)
   */
  explicit OMIThumbnail(QString imageUrl, QSize displaySize, QNetworkAccessManager* networkManager, QWidget* parent = nullptr); 
};

class OnlineModItem : public QWidget {
  Q_OBJECT
private:
  OnlineMod m_mod;
  QNetworkAccessManager* m_nm; // "Owned" by OnlineModsIndexScreen
  OMIThumbnail* m_thumbnail = nullptr;
public:
  explicit OnlineModItem(QNetworkAccessManager* networkManager, OnlineMod mod, QWidget* parent = nullptr);
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
};

}

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

  QWidget* m_footer = nullptr;

  int m_perPage = 18;
  int m_page = 0;
  int m_totalItems = 0;
  int m_totalPages = 1;


  bool m_firstShown = false;

  std::vector<detail::OnlineModItem*> m_whar;

public:
  explicit OnlineModsIndexScreen(Router* router, QWidget* parent = nullptr);

  void showEvent(QShowEvent* event) override;
signals:
  void totalItemsUpdated();
  void pageNumberUpdated();
protected:
  void queryIndex();

  void setTotalItems(int value);
  void setPage(int page);
};
