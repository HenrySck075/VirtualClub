#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QStandardPaths>
#include <QComboBox>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include "../ui/GradientBackground.hpp"
#include "../ui/MESWidgets.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "utils/BackgroundLoader.hpp"
#include "utils/FlowLayout.hpp"
#include "ui/IconButton.hpp"
#include "ui/PixmapWidget.hpp"
#include "ui/Router.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/i18n.hpp"
#include "utils/utils.hpp"

class BackgroundPackItem : public QWidget {
  Q_OBJECT
  QString m_pack;
  QList<QPixmap> m_pixmaps;
  size_t m_appliedPixmapIndex = 0;
  size_t m_currentPixmapIndex = 0;
  bool m_selected = false;
  bool m_hovered = false;

  QTimer* m_previewCycleTimer = nullptr;
  QLabel* m_preview = nullptr;

public:
  explicit BackgroundPackItem(QString pack, QWidget* parent = nullptr)
    : QWidget(parent), m_pack(std::move(pack)) {
    setFixedSize(250, 174);
    setCursor(Qt::PointingHandCursor);
    setToolTip(m_pack);

    m_previewCycleTimer = new QTimer(this);
    m_previewCycleTimer->setInterval(500); 

    connect(m_previewCycleTimer, &QTimer::timeout, this, std::bind(&BackgroundPackItem::updatePreview, this));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    auto images = BackgroundLoader::getImages(m_pack);
    auto currentImage = BackgroundLoader::getImage(m_pack);
    if (!currentImage.isEmpty()) {
      m_appliedPixmapIndex = m_currentPixmapIndex = images.indexOf(currentImage);
    }

    m_preview = new QLabel(this);
    m_preview->setFixedSize(240, 135);
    m_preview->setAlignment(Qt::AlignCenter);

    for (const auto& image : images) {
      m_pixmaps.append(QPixmap(image).scaled(
        m_preview->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation
      ));
    }

    m_preview->setPixmap(m_pixmaps[m_currentPixmapIndex]);
    layout->addWidget(m_preview);

    auto* name = new QLabel(m_pack, this);
    name->setAlignment(Qt::AlignCenter);
    name->setFont(QFont("Quicksand", 11, QFont::Weight::DemiBold));
    layout->addWidget(name);

    for (auto* child : findChildren<QWidget*>()) {
      child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  }

  void setSelected(bool selected) {
    m_selected = selected;
    update();
  }

  const QString& pack() const {
    return m_pack;
  }

signals:
  void clicked();

protected:
  void enterEvent(QEnterEvent* event) override {
    m_hovered = true;
    if (m_previewCycleTimer) m_previewCycleTimer->start();
    update();
    QWidget::enterEvent(event);
  }

  void leaveEvent(QEvent* event) override {
    m_hovered = false;
    if (m_previewCycleTimer) m_previewCycleTimer->stop();
    m_currentPixmapIndex = m_appliedPixmapIndex - 1;
    updatePreview();
    update();
    QWidget::leaveEvent(event);
  }

  void updatePreview() {
    if (m_pixmaps.isEmpty()) return;

    m_currentPixmapIndex = (m_currentPixmapIndex + 1) % m_pixmaps.size();
    if (m_preview) {
      m_preview->setPixmap(m_pixmaps[m_currentPixmapIndex]);
    }
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      emit clicked();
      event->accept();
      return;
    }
    QWidget::mouseReleaseEvent(event);
  }

  void paintEvent(QPaintEvent* event) override {
    QWidget::paintEvent(event);
    if (!m_hovered && !m_selected) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
    gradient.setColorAt(0.0, m_selected ? c_primaryColor : c_primaryLightColor);
    gradient.setColorAt(1.0, m_selected ? c_secondaryColor : c_secondaryLightColor);
    painter.setPen(QPen(gradient, m_selected ? 4 : 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 5, 5);
  }
};

SettingsScreen::SettingsScreen(Router *router, QWidget *parent) : QWidget(parent) {
  setWindowTitle("Settings");
  // Set up the layout for the Settings screen
  auto *layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(0);

  auto* scrollArea = new QScrollArea(this);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  // TODO: a side effect of this is that this affects background color of QLineEdit
  // and i cant target the viewport specifically through object name for some reason
  // it looks okay though
  scrollArea->viewport()->setStyleSheet("background: transparent;");
  scrollArea->viewport()->setAutoFillBackground(true);
  setupGradientWidget(scrollArea);

  auto* content = new QWidget();
  content->setAutoFillBackground(true);
  scrollArea->setWidget(content);
  layout->addWidget(scrollArea);
  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 16;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignTop);
  contentLayout->setHorizontalSizeConstraint(QLayout::SetMaximumSize);

  auto notesLabel = new QLabel();
  notesLabel->setStyleSheet("font-style: italic;");
  addRetranslateCallback(this, [this, notesLabel] {
    notesLabel->setText(tr("All changes are automatically saved."));
  });
  contentLayout->addWidget(notesLabel,0,Qt::AlignLeft);

  if (router) {
    auto *globalSettingsButton = new Button(LucideIcons::settings, content);
    contentLayout->addWidget(globalSettingsButton, 0, Qt::AlignLeft);
    addRetranslateCallback(this, [this, globalSettingsButton] {
      globalSettingsButton->setText(tr("Global settings"));
    });
    connect(globalSettingsButton, &Button::clicked, this, [router]() {
      router->navigate("/settings/global");
    });
  }

  auto addSettingsHeader = [this, contentLayout](QString headerName) {
    auto* headerLabel = new QLabel(headerName);
    addRetranslateCallback(this, [this, headerLabel, headerName] {
      headerLabel->setText(tr(headerName.toStdString().c_str()));
    });
    headerLabel->setFont(QFont("Quicksand", 18, QFont::Bold));
    contentLayout->addWidget(headerLabel, 0, Qt::AlignLeft);
    // pad it out veritcally
    headerLabel->setContentsMargins(0, 16, 0, 12);
    return headerLabel;
  };

  // Add a label or any other widgets you want to display on the Settings screen
  auto addSettingsLabel = [this, contentLayout](QString name, QString desc, QWidget* action, std::function<void(QHBoxLayout*, QVBoxLayout*)> extra = nullptr) {
    auto layout = new QHBoxLayout();
    contentLayout->addLayout(layout);
    layout->setAlignment(Qt::AlignLeft);

    auto metadataLayout = new QVBoxLayout();
    layout->addLayout(metadataLayout);

    auto* nameLabel = new QLabel();
    nameLabel->setFont(QFont("Quicksand", 12, QFont::Bold)); 
    metadataLayout->addWidget(nameLabel); 

    auto* descLabel = new QLabel();
    descLabel->setFont(QFont("Quicksand", 10)); 
    descLabel->setWordWrap(true); 
    descLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    metadataLayout->addWidget(descLabel); 

    addRetranslateCallback(this, [this, nameLabel, name, descLabel, desc] {
      nameLabel->setText(tr(name.toStdString().c_str()));
      descLabel->setText(tr(desc.toStdString().c_str()));
    });

    layout->addWidget(action);

    if (extra) extra(layout, metadataLayout);

    return std::make_pair(nameLabel, descLabel);
  };


  auto settings = ProfileSettings::get();

  addSettingsHeader(QT_TR_NOOP("Profile"));

  auto* pfpChangeButton = new IconButton(LucideIcons::pen);

  addSettingsLabel(
    QT_TR_NOOP("Profile picture"),
    QT_TR_NOOP("Give it a cooler image"),
    pfpChangeButton,
    [this, settings, pfpChangeButton](QHBoxLayout* layout, QVBoxLayout* metadataLayout){
      auto* pfpLabel = new PixmapWidget;
      pfpLabel->setPixmap(QPixmap(settings->value(STK_PFP, ":/defaultuserprofile.png").toString()));
      pfpLabel->setFixedSize({64,64});
      layout->insertWidget(0, pfpLabel);

      connect(pfpChangeButton, &Button::clicked, this, [this, settings, pfpLabel](){
        if (askForProfilePictureChange()) {
          pfpLabel->setPixmap(QPixmap(settings->value(STK_PFP, ":/defaultuserprofile.png").toString()));
        }
      });
    }
  );


  auto* displayNameTextInput = new QLineEdit();
  displayNameTextInput->setText(
    settings->value(STK_DISPLAYNAME, ProfileSingleApp::instance()->profileId()).toString()
  );
  displayNameTextInput->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  bool haveDisplayName = settings->contains(STK_DISPLAYNAME);
  auto [dnNameLabel, dnDescLabel] = addSettingsLabel(
    QT_TR_NOOP("Display name"), 
    haveDisplayName
      ? QT_TR_NOOP("Change the profile's display name.")
      : QT_TR_NOOP("you dont want the name to look like that, do you?"), 
    displayNameTextInput
  );
  connect(displayNameTextInput, &QLineEdit::editingFinished, this, [displayNameTextInput, dnDescLabel, haveDisplayName, settings](){
    settings->setValue(STK_DISPLAYNAME, displayNameTextInput->text());
    if (!haveDisplayName) 
      dnDescLabel->setText("great! :D");

    getMainWindow()->setPageTitleBar("Settings");
  });



  auto* basePathChangeButton = new Button(LucideIcons::folder_pen, content);
  addSettingsLabel(
    QT_TR_NOOP("Base game's path"),
    QT_TR_NOOP("The directory where the base game is installed."),
    basePathChangeButton,
    [this, settings, basePathChangeButton](QHBoxLayout*, QVBoxLayout* metadataLayout) {
      auto* basePath = new QLineEdit(this);
      basePath->setText(settings->value(STK_BASEPATH).toString());
      basePath->setReadOnly(true);
      metadataLayout->addWidget(basePath);

      connect(basePathChangeButton, &Button::clicked, this, [this, settings, basePath](){
        if (askForBasePathChange()) {
          basePath->setText(settings->value(STK_BASEPATH).toString());
        }
      });
    }
  );
  addRetranslateCallback(this, [basePathChangeButton, this] {
    basePathChangeButton->setText(tr("Change"));
  });


  addSettingsHeader(QT_TR_NOOP("Application"));

  auto* languagesComboBox = new QComboBox();
  {
    auto currentLanguage = QLocale(settings->value(STK_LANGUAGE, "en").toString());
    int idx = 0;
    for (auto& l : getAvailableLocales()) {
      languagesComboBox->addItem(l.nativeLanguageName(), l);

      if (currentLanguage == l) languagesComboBox->setCurrentIndex(idx);
      idx++;
    }
  }

  connect(languagesComboBox, &QComboBox::currentIndexChanged, this, [this, languagesComboBox, settings](int index){
    auto locale = languagesComboBox->itemData(index).value<QLocale>();
    
    setLanguage(locale);
    settings->setValue(STK_LANGUAGE, locale.name());
  });
  
  auto [lNameLabel, lDescLabel] = addSettingsLabel(
    QT_TR_NOOP("Language"), 
    QT_TR_NOOP("Sets the application language"), 
    languagesComboBox
  );

  {
    auto* nameLabel = new QLabel(tr("Background"));
    nameLabel->setFont(QFont("Quicksand", 12, QFont::Bold)); 
    contentLayout->addWidget(nameLabel); 
  }

  auto* backgroundScroll = new QScrollArea(content);
  backgroundScroll->setFrameShape(QFrame::NoFrame);
  backgroundScroll->setWidgetResizable(true);
  backgroundScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  backgroundScroll->setFixedHeight(208);
  backgroundScroll->viewport()->setStyleSheet("background: transparent;");
  auto* backgroundContent = new QWidget();
  backgroundContent->setStyleSheet(
    QString("QScrollArea {background: transparent; border: 2px solid %1; border-radius: 4px;}")
    .arg(qColorToCSSrgb(c_primaryColor))
  );
  auto* backgroundLayout = new FlowLayout(backgroundContent, 8, 8, 8);
  backgroundLayout->setAlignment(Qt::AlignHCenter);
  backgroundScroll->setWidget(backgroundContent);
  contentLayout->addWidget(backgroundScroll);

  const auto selectedPack = settings->value(STK_BGPACK).toString();
  for (const auto& pack : BackgroundLoader::getPacks()) {
    auto* item = new BackgroundPackItem(pack, backgroundContent);
    item->setSelected(pack == selectedPack);
    backgroundLayout->addWidget(item);
    connect(item, &BackgroundPackItem::clicked, this, [this, settings, item, backgroundContent] {
      settings->setValue(STK_BGPACK, item->pack());
      for (auto* other : backgroundContent->findChildren<BackgroundPackItem*>()) {
        other->setSelected(other == item);
      }
      if (auto* mainWindow = getMainWindow()) {
        mainWindow->setBackgroundPack(item->pack());
      }
    });
  }

}

#include "Settings.moc"
