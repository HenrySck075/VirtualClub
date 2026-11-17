#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QStandardPaths>
#include <QComboBox>
#include "../ui/GradientBackground.hpp"
#include "../ui/MESWidgets.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "ui/IconButton.hpp"
#include "ui/PixmapWidget.hpp"
#include "ui/Router.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/i18n.hpp"
#include "utils/utils.hpp"

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
    qDebug() << "set language to" << locale << "hopefully";
    setLanguage(locale);
    settings->setValue(STK_LANGUAGE, locale.name());
  });
  
  auto [lNameLabel, lDescLabel] = addSettingsLabel(
    QT_TR_NOOP("Language"), 
    QT_TR_NOOP("Sets the application language"), 
    languagesComboBox
  );
}
