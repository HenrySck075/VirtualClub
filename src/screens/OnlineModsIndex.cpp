#include "OnlineModsIndex.hpp"
#include "MainWindow.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainterPath>
#include <QLabel>
#include <QLineEdit>
#include "ui/ElidedLabel.hpp"
#include "ui/GradientBackground.hpp"
#include "ui/IconButton.hpp"
#include "ui/ImageLoadingWidget.hpp"
#include "ui/MESWidgets.hpp"
#include "utils/FlowLayout.hpp"
#include "utils/LucideIcons.hpp"
#include "ui/Router.hpp"
#include "ui/LoadingCircle.hpp"
#include "utils/OnlineModsIndex.hpp"
#include "utils/utils.hpp"
#include <QComboBox>
#include <QMouseEvent>
#include <QPointer>

class OnlineModInfoDC : public DialogContent {
  Q_OBJECT
private:
  OnlineMod m_mod;
  QNetworkAccessManager* m_nm = nullptr;
public:
  explicit OnlineModInfoDC(QNetworkAccessManager* networkManager, OnlineMod mod, QWidget* parent = nullptr)
    : DialogContent(parent), m_nm(networkManager), m_mod(mod) {
    setFixedWidth(800);
    setMaximumHeight(580);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setAlignment(Qt::AlignTop);

    std::vector<std::string> imageUrls;
    if (!mod.imageUrl.empty()) {
      imageUrls.push_back(mod.imageUrl);
    }
    imageUrls.insert(imageUrls.end(), mod.screenshotImageUrls.begin(), mod.screenshotImageUrls.end());
    if (imageUrls.empty()) {
      imageUrls.emplace_back();
    }

    auto* carouselLayout = new QHBoxLayout();
    carouselLayout->setContentsMargins(0, 0, 0, 0);
    carouselLayout->setSpacing(8);

    auto* previousImageButton = new IconButton(LucideIcons::arrow_left, this);
    auto* imageScrollArea = new QScrollArea(this);
    imageScrollArea->setFixedSize(580, 320);
    imageScrollArea->setFrameShape(QFrame::NoFrame);
    imageScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    imageScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    imageScrollArea->setWidgetResizable(false);

    auto* imageContainer = new QWidget();
    auto* imageLayout = new QHBoxLayout(imageContainer);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(0);
    for (const auto& imageUrl : imageUrls) {
      auto* image = new ImageLoadingWidget(QString::fromStdString(imageUrl), m_nm, imageContainer);
      image->setFixedSize(580, 320);
      imageLayout->addWidget(image);
    }
    imageScrollArea->setWidget(imageContainer);

    auto* nextImageButton = new IconButton(LucideIcons::arrow_right, this);
    carouselLayout->addWidget(previousImageButton);
    carouselLayout->addWidget(imageScrollArea);
    carouselLayout->addWidget(nextImageButton);
    layout->addLayout(carouselLayout);

    auto* imageScrollBar = imageScrollArea->horizontalScrollBar();
    auto updateImageButtons = [imageScrollBar, previousImageButton, nextImageButton]() {
      previousImageButton->setEnabled(imageScrollBar->value() > 0);
      nextImageButton->setEnabled(
        imageScrollBar->value() < imageScrollBar->maximum()
      );
    };
    connect(previousImageButton, &QPushButton::clicked, this, [imageScrollArea, imageScrollBar, updateImageButtons]() {
      imageScrollBar->setValue(imageScrollBar->value() - imageScrollArea->viewport()->width());
      updateImageButtons();
    });
    connect(nextImageButton, &QPushButton::clicked, this, [imageScrollArea, imageScrollBar, updateImageButtons]() {
      imageScrollBar->setValue(imageScrollBar->value() + imageScrollArea->viewport()->width());
      updateImageButtons();
    });
    connect(imageScrollBar, &QScrollBar::valueChanged, this, [updateImageButtons]() {
      updateImageButtons();
    });
    updateImageButtons();

    auto* titleLabel = new ElidedLabel(QString::fromStdString(mod.name), this);
    titleLabel->setFont(QFont("Quicksand", 18, QFont::Weight::Bold));
    titleLabel->setMaxLines(2);
    layout->addWidget(titleLabel);

    auto* authorLabel = new QLabel(QString::fromStdString(mod.author), this);
    authorLabel->setFont(QFont("Quicksand", 15, QFont::Weight::DemiBold));
    layout->addWidget(authorLabel);

    layout->addSpacing(12);

    auto* descriptionScroll = new QScrollArea(this);
    auto* descriptionLabel = new QLabel(QString::fromStdString(mod.description), this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setFont(QFont("Quicksand", 12));
    descriptionScroll->setWidget(descriptionLabel);
    descriptionScroll->setWidgetResizable(true);
    layout->addWidget(descriptionScroll);
  }
};

class OMIThumbnail : public QWidget {
  Q_OBJECT
private:
  bool m_hovered = false;

  QNetworkAccessManager* m_nm = nullptr;

  ImageLoadingWidget* m_child = nullptr;
  
  const int m_padding = 8;
  const int m_radius = 4;
public:
  void setHovered(bool hovered){
    m_hovered = hovered;
    update();
  }

  void resizeEvent(QResizeEvent* event) override {
    updateChildRender();
  }
  void paintEvent(QPaintEvent* event) override {
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
  void updateChildRender() {
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

  /*
   * @params displaySize image display size (including padding for the hover border)
   */
  explicit OMIThumbnail(QString imageUrl, QSize displaySize, QNetworkAccessManager* networkManager, QWidget* parent = nullptr)
    : QWidget(parent), m_nm(networkManager) {
    setFixedSize(displaySize);
    m_child = new ImageLoadingWidget(imageUrl, m_nm, this);

    updateChildRender();
  }
};

class OnlineModItem : public QWidget {
  Q_OBJECT
private:
  OnlineMod m_mod;
  QNetworkAccessManager* m_nm; // "Owned" by OnlineModsIndexScreen
  OMIThumbnail* m_thumbnail = nullptr;
public:
  explicit OnlineModItem(QNetworkAccessManager* networkManager, OnlineMod mod, QWidget* parent = nullptr)
    : QWidget(parent), m_nm(networkManager), m_mod(mod) {
    setFixedWidth(258);
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop);

    // OMIThumbnail (16:9 ratio), title and author
    m_thumbnail = new OMIThumbnail(QString::fromStdString(mod.imageUrl), QSize(250, 140), m_nm, this);
    layout->addWidget(m_thumbnail);

    auto* titleLabel = new ElidedLabel(QString::fromStdString(mod.name), this);
    //titleLabel->setWordWrap(true);
    titleLabel->setFont(QFont("Quicksand", 12, QFont::Weight::DemiBold));
    titleLabel->setMaxLines(2);
    layout->addWidget(titleLabel);

    auto* authorLabel = new QLabel(QString::fromStdString(mod.author), this);
    authorLabel->setFont(QFont("Quicksand", 10));
    layout->addWidget(authorLabel);

    for (auto* child : findChildren<QWidget*>()) {
      child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  }
  void enterEvent(QEnterEvent* event) override {
    m_thumbnail->setHovered(true);
    QWidget::enterEvent(event);
  }
  void leaveEvent(QEvent* event) override {
    m_thumbnail->setHovered(false);
    QWidget::leaveEvent(event);
  }
  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      Dialog::showContentDialog(
        getMainWindow(),
        "Mod information",
        new OnlineModInfoDC(m_nm, m_mod)
      );
      event->accept();
      return;
    }
    QWidget::mouseReleaseEvent(event);
  }
};


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
    queryIndex();
  });
  m_currentIndex = indexes[0];

  headerLayout->addWidget(indexSelector);

  headerLayout->addWidget(new QLabel("Sort by", header));

  auto* sortSelector = new QComboBox(header);
  std::array<std::pair<ListSorting, std::string>, 5> sorts {{
    {ListSorting::Newest, "Newest"},
    {ListSorting::Oldest, "Oldest"},
    {ListSorting::TitleAZ, "Title A-Z"},
    {ListSorting::TitleZA, "Title Z-A"},
    {ListSorting::Author, "Author"},
  }};

  for (const auto& [sort, name] : sorts) {
    sortSelector->addItem(QString::fromStdString(name));
  }

  connect(sortSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, sortSelector, sorts](int idx){
    if (idx < 0 || idx >= sorts.size()) return;
    auto sort = sorts[idx].first;
    m_sort = sort;
    queryIndex();
  });

  headerLayout->addWidget(sortSelector);

  
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

  m_exceptionWidget = new QWidget();
  auto* exceptionLayout = new QVBoxLayout(m_exceptionWidget);
  m_exceptionLabel = new QLabel();
  m_exceptionLabel->setWordWrap(true);
  m_exceptionLabel->setAlignment(Qt::AlignCenter);
  exceptionLayout->addWidget(m_exceptionLabel, 0, Qt::AlignCenter);
  m_container->addWidget(m_exceptionWidget);


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

  auto* totalPagesLabel = new QLabel(footer); // text content set below
  footerLayout->addWidget(totalPagesLabel);

  auto* perPageInput = new QLineEdit(footer);
  perPageInput->setFixedWidth(40);
  perPageInput->setAlignment(Qt::AlignCenter);
  perPageInput->setText(QString::number(m_perPage));
  connect(perPageInput, &QLineEdit::editingFinished, [this, perPageInput](){
      bool ok;
      int newPerPage = perPageInput->text().toInt(&ok); 
      if (ok && newPerPage > 0 && newPerPage <= m_totalPages) {
        setPerPage(newPerPage);
      } else {
        perPageInput->setText(QString::number(m_perPage)); 
      }
  });
  footerLayout->addWidget(perPageInput);

  footerLayout->addWidget(new QLabel("mods per page", footer));
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



  connect(this, &OnlineModsIndexScreen::pageNumberUpdated, [pageInput, firstPageBtn, prevPageBtn, nextPageBtn, lastPageBtn, this](){
    auto nv = m_page;
    pageInput->setText(QString::number(nv + 1));
    queryIndex();

    // disable navigation buttons based on the value
    firstPageBtn->setEnabled(nv > 0);
    prevPageBtn->setEnabled(nv > 0);
    nextPageBtn->setEnabled(nv + 1 < m_totalPages);
    lastPageBtn->setEnabled(nv + 1 < m_totalPages);
  });

  connect(this, &OnlineModsIndexScreen::perPageNumberUpdated, [this](){
    m_totalPages = ceil(m_totalItems / m_perPage);
    setPage(m_page >= m_totalPages ? m_totalPages - 1 : m_page);
    queryIndex();
  });

  firstPageBtn->setEnabled(false);
  prevPageBtn->setEnabled(false);

  connect(this, &OnlineModsIndexScreen::totalItemsUpdated, [this, totalPagesLabel, updateModsCountLabel](){
    m_totalPages = ceil(m_totalItems / m_perPage);
    totalPagesLabel->setText(QString("of %1, showing").arg(m_totalPages));
    updateModsCountLabel();
  });
}

void OnlineModsIndexScreen::queryIndex() {
  if (m_currentIndex != nullptr) {
    if (m_container) m_container->setCurrentWidget(m_placeholderWidget);
    if (m_footer) m_footer->hide();
    m_currentIndex->getMods({
      .page = m_page, .perPage = m_perPage, .sorting = m_sort
    }).then(this, [this](std::vector<OnlineMod> list){
      for (auto* item : m_whar) {
        m_contentLayout->removeWidget(item);
        item->deleteLater();
      }
      m_whar.clear();

      for (const auto& mod : list) {
        auto* item = new OnlineModItem(m_nm, mod, m_content);
        m_contentLayout->addWidget(item);
        m_whar.push_back(item);
      }

      setTotalItems(m_currentIndex->getTotalMods());

      if (m_container) m_container->setCurrentWidget(m_scrollArea);
      if (m_footer && m_totalItems > m_perPage) m_footer->show();
    }).onFailed(this, [this](const std::exception& e){
      m_exceptionLabel->setText(QString("Failed to fetch mods: %1").arg(e.what()));
      if (m_container) m_container->setCurrentWidget(m_exceptionWidget);
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
  if (value == m_totalItems) return;
  m_totalItems = value;
  emit totalItemsUpdated();
}
void OnlineModsIndexScreen::setPage(int page) {
  if (page == m_page) return;
  if (page >= 0 && page < m_totalPages) {
    m_page = page;
    emit pageNumberUpdated();
  }
}
void OnlineModsIndexScreen::setPerPage(int value) {
  if (value == m_perPage) return;
  // why 12?
  // why would you want anything smaller than this?
  if (value > 12 && value < m_totalItems) {
    m_perPage = value;
    emit perPageNumberUpdated();
  }
}

#include "OnlineModsIndex.moc"
