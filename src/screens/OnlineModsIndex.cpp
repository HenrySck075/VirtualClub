#include "OnlineModsIndex.hpp"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPainterPath>
#include <QLabel>
#include <QLineEdit>
#include "ui/GradientBackground.hpp"
#include "ui/IconButton.hpp"
#include "ui/PixmapWidget.hpp"
#include "utils/EventFilters.hpp"
#include "utils/FlowLayout.hpp"
#include "utils/LucideIcons.hpp"
#include "ui/Router.hpp"
#include "ui/LoadingCircle.hpp"
#include "utils/OnlineModsIndex.hpp"
#include "utils/utils.hpp"
#include <QComboBox>
#include <QPointer>

namespace detail {

OMIThumbnail::OMIThumbnail(QString imageUrl, QSize displaySize, QNetworkAccessManager* networkManager, QWidget* parent) 
: QWidget(parent), m_nm(networkManager)
{
  setFixedSize(displaySize);
  auto* container = new QStackedWidget(this);
  m_child = container;
  // gray background with `new LoadingCircle()` in the center
  auto* placeholderWidget = new QWidget();
  placeholderWidget->setStyleSheet("background-color: #999999;");
  container->addWidget(placeholderWidget);

  auto* placeholderLayout = new QVBoxLayout(placeholderWidget);
  placeholderLayout->setContentsMargins(m_padding, m_padding, m_padding, m_padding);

  updateChildRender();

  if (!imageUrl.isEmpty()) {
    auto* loadingCircle = new LoadingCircle(placeholderWidget);
    loadingCircle->setColor(Qt::white);
    placeholderLayout->addWidget(loadingCircle, 0, Qt::AlignCenter);


    fetchUrlAsync(m_nm, QUrl(imageUrl)).then([this, container](QByteArray data) {
      QPixmap pixmap;
      if (pixmap.loadFromData(data)) {
        auto* pixmapWidget = new PixmapWidget(this);
        pixmapWidget->setPixmap(pixmap);
        container->addWidget(pixmapWidget);
        container->setCurrentWidget(pixmapWidget);
      } else {
        // If the image fails to load, keep the placeholder
        qWarning("Failed to load image from data.");
      }
    });
  }
}

void OMIThumbnail::resizeEvent(QResizeEvent* e) {
  updateChildRender();
}
void OMIThumbnail::updateChildRender() {
  // Ensure padding doesn't result in negative dimensions if the window gets too small
  int childX = m_padding;
  int childY = m_padding;
  int childWidth = qMax(0, width() - (2 * m_padding));
  int childHeight = qMax(0, height() - (2 * m_padding));

  // Apply the padded geometry to the child
  m_child->setGeometry(childX, childY, childWidth, childHeight);

  QPainterPath path;
  path.addRoundedRect(m_child->rect(), m_radius, m_radius);
  QRegion roundedRegion(path.toFillPolygon().toPolygon());

  m_child->setMask(roundedRegion);
}
void OMIThumbnail::paintEvent(QPaintEvent* event) {
  // Draw a rounded rectangle at `this->rect()` with *only* a 4px outline, 
  // using gradient color of c_primaryLightColor at the top-left and c_secondaryLightColor at the bottom-right

  if (m_hovered) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
    gradient.setColorAt(0.0, c_primaryLightColor);
    gradient.setColorAt(1.0, c_secondaryLightColor);

    QPen pen(gradient, m_radius);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Inset by half the pen width (2px) so the 4px border stays inside the widget bounds
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 4, 4);
  }
}

void OMIThumbnail::setHovered(bool hovered) {
  m_hovered = hovered;
  update();
}


OnlineModItem::OnlineModItem(QNetworkAccessManager* networkManager, OnlineMod mod, QWidget* parent) 
  : QWidget(parent), m_nm(networkManager), m_mod(mod)
{
  setFixedWidth(258);

  auto* layout = new QVBoxLayout(this);
  layout->setSpacing(4);
  layout->setAlignment(Qt::AlignTop);

  // OMIThumbnail (16:9 ratio), title and author
  m_thumbnail = new OMIThumbnail(QString::fromStdString(mod.imageUrl), QSize(250, 140), m_nm, this);
  layout->addWidget(m_thumbnail);

  auto* titleLabel = new QLabel(QString::fromStdString(mod.name), this);
  titleLabel->setWordWrap(true);
  titleLabel->setFont(QFont("Quicksand", 12, QFont::Weight::DemiBold));
  layout->addWidget(titleLabel);

  auto* authorLabel = new QLabel(QString::fromStdString(mod.author), this);
  authorLabel->setFont(QFont("Quicksand", 10));
  layout->addWidget(authorLabel);
}
void OnlineModItem::enterEvent(QEnterEvent* event) {
  m_thumbnail->setHovered(true);
}
void OnlineModItem::leaveEvent(QEvent* event) {
  m_thumbnail->setHovered(false);
}

}


OnlineModsIndexScreen::OnlineModsIndexScreen(Router* router, QWidget* parent) : QWidget(parent) {
  m_router = router;
  m_nm = createNetworkAccessManager(this);

  setStyleSheet(
    "#header {background-color: #fff7e7;} #footer {background-color: white;}"
  );

  setWindowTitle("Browse Mods");
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;

  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);
  layout->setAlignment(Qt::AlignTop);

  auto* header = new QWidget(this);
  header->setObjectName("header");
  header->setFixedHeight(40);
  header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(header);

  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(8, 0, 8, 0);
  headerLayout->setAlignment(Qt::AlignLeft);

  auto* backButton = new IconButton(LucideIcons::arrow_left);
  connect(backButton, &IconButton::clicked, [this](){
    m_router->navigate("/mods");
  });
  headerLayout->addWidget(backButton);

  headerLayout->addStretch(1);

  auto* modsCountLabel = new QLabel();
  headerLayout->addWidget(modsCountLabel);
  auto updateModsCountLabel = [this, modsCountLabel](){
    modsCountLabel->setText(QString("Mods: %1").arg(m_totalItems));
  };
  updateModsCountLabel();

  auto* indexSelector = new QComboBox(header);
  QList<QPointer<OnlineIndex>> indexes {{
    QPointer<OnlineIndex>(new ModdingCentralIndex(this)),
    QPointer<OnlineIndex>(new DokiModsDotMeIndex(this))
  }}; 

  for (auto& index : indexes) {
    indexSelector->addItem(index->displayName());
  }

  connect(indexSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, indexes](int idx){
    if (idx < 0 || idx >= indexes.size()) return;
    auto index = indexes[idx];
    if (!index) return;
    m_currentIndex = index;
  });
  m_currentIndex = indexes[0];

  headerLayout->addWidget(indexSelector);

  
  m_container = new QStackedWidget(this);
  setupGradientWidget(m_container);
  m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(m_container);

  m_scrollArea = new QScrollArea(m_container);
  m_content = new QWidget();
  m_content->setObjectName("content");
  m_scrollArea->setWidget(m_content);
  m_scrollArea->setStyleSheet("QScrollArea { background: transparent; }");
  m_content->setStyleSheet("#content { background: transparent; }");
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_contentLayout = new FlowLayout(m_content);
  m_contentLayout->setSpacing(8);
  m_contentLayout->setAlignment(Qt::AlignHCenter);
  m_container->addWidget(m_scrollArea);


  m_placeholderWidget = new QWidget();
  auto* loadingCircle = new LoadingCircle(m_placeholderWidget);
  loadingCircle->setScale(2);
  loadingCircle->setColor(QColorConstants::Black);
  auto* pContentLayout = new QVBoxLayout(m_placeholderWidget);
  pContentLayout->addWidget(loadingCircle, 0, Qt::AlignCenter);
  m_container->addWidget(m_placeholderWidget);




  auto* footer = new QWidget(this);
  footer->setObjectName("footer");
  footer->setFixedHeight(40);
  footer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(footer);
  m_footer = footer;

  auto* footerLayout = new QHBoxLayout(footer);
  footerLayout->setAlignment(Qt::AlignCenter);
  footerLayout->setContentsMargins(10, 0, 10, 0);
  footerLayout->setSpacing(8); 

  auto* firstPageBtn = new IconButton(LucideIcons::chevrons_left);
  connect(firstPageBtn, &IconButton::clicked, [this](){
      setPage(0);
  });
  footerLayout->addWidget(firstPageBtn);

  auto* prevPageBtn = new IconButton(LucideIcons::chevron_left);
  connect(prevPageBtn, &IconButton::clicked, [this](){
      if (m_page > 0) {
        setPage(m_page - 1);
      }
  });
  footerLayout->addWidget(prevPageBtn);

  footerLayout->addStretch(1);

  footerLayout->addWidget(new QLabel("Page", footer));

  auto* pageInput = new QLineEdit(footer);
  pageInput->setFixedWidth(40);
  pageInput->setAlignment(Qt::AlignCenter);
  pageInput->setText(QString::number(m_page + 1));
  connect(pageInput, &QLineEdit::editingFinished, [this, pageInput](){
      bool ok;
      int newPage = pageInput->text().toInt(&ok);
      if (ok && newPage > 0 && newPage <= m_totalPages) {
        setPage(newPage - 1); // Convert to 0-based index
      } else {
        pageInput->setText(QString::number(m_page + 1)); // Reset to current page
      }
  });
  footerLayout->addWidget(pageInput);

  auto* totalPagesLabel = new QLabel(QString("of %1").arg(m_totalPages), footer);
  footerLayout->addWidget(totalPagesLabel);
  footerLayout->addStretch(1);

  auto* nextPageBtn = new IconButton(LucideIcons::chevron_right);
  connect(nextPageBtn, &IconButton::clicked, [this](){
      if (m_page + 1 < m_totalPages) {
        setPage(m_page + 1);
      }
  });
  footerLayout->addWidget(nextPageBtn);

  auto* lastPageBtn = new IconButton(LucideIcons::chevrons_right);
  connect(lastPageBtn, &IconButton::clicked, [this](){
      if (m_page + 1 < m_totalPages) {
        setPage(m_totalPages - 1);
      }
  });
  footerLayout->addWidget(lastPageBtn);



  connect(this, &OnlineModsIndexScreen::pageNumberUpdated, [&](){
    auto nv = m_page;
    pageInput->setText(QString::number(nv + 1));
    queryIndex();

    // disable navigation buttons based on the value
    firstPageBtn->setEnabled(nv > 0);
    prevPageBtn->setEnabled(nv > 0);
    nextPageBtn->setEnabled(nv + 1 < m_totalPages);
    lastPageBtn->setEnabled(nv + 1 < m_totalPages);
  });

  firstPageBtn->setEnabled(false);
  prevPageBtn->setEnabled(false);

  connect(this, &OnlineModsIndexScreen::totalItemsUpdated, [this, totalPagesLabel, updateModsCountLabel](){
    m_totalPages = ceil(m_totalItems / m_perPage);
    totalPagesLabel->setText(QString("of %1").arg(m_totalPages));
    updateModsCountLabel();
  });
}

void OnlineModsIndexScreen::queryIndex() {
  if (m_currentIndex != nullptr) {
    if (m_container) m_container->setCurrentWidget(m_placeholderWidget);
    if (m_footer) m_footer->hide();
    m_currentIndex->getMods({.page = m_page, .perPage = m_perPage}).then(this, [this](std::vector<OnlineMod> list){
      // remove all widgets in m_contentLayout
      findChildWidgetBy(m_contentLayout, [this](QWidget* w){ m_contentLayout->removeWidget(w); return false; });

      for (const auto& mod : list) {
        auto* item = new detail::OnlineModItem(m_nm, mod, m_content);
        m_contentLayout->addWidget(item);
      }

      setTotalItems(m_currentIndex->getTotalMods());

      if (m_container) m_container->setCurrentWidget(m_scrollArea);
      if (m_footer && m_totalItems > m_perPage) m_footer->show();
    });
  }
}


void OnlineModsIndexScreen::showEvent(QShowEvent* event) {
  if (!m_firstShown) {
    queryIndex();
  }
  m_firstShown = true;
};

void OnlineModsIndexScreen::setTotalItems(int value) {
  m_totalItems = value;
  emit totalItemsUpdated();
}
void OnlineModsIndexScreen::setPage(int page) {
  if (page >= 0 && page < m_totalPages) {
    m_page = page;
    emit pageNumberUpdated();
  }
}
