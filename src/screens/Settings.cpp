#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <qfiledialog.h>
#include <qstandardpaths.h>
#include "../ui/GradientBackground.hpp"
#include "../ui/MESWidgets.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "ui/IconButton.hpp"
#include "ui/PixmapWidget.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/utils.hpp"

SettingsScreen::SettingsScreen(QWidget *parent) : QWidget(parent) {
  setWindowTitle("Settings");
  // Set up the layout for the Settings screen
  auto *layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(0);

  auto* content = new GradientBackground(this);
  layout->addWidget(content);
  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 16;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignTop);
  contentLayout->setHorizontalSizeConstraint(QLayout::SetMaximumSize);

  contentLayout->addWidget(new QLabel("<i>All changes are automatically saved.</i>"),0,Qt::AlignLeft);

  auto addSettingsHeader = [contentLayout](QString headerName) {
    auto* headerLabel = new QLabel(headerName);
    headerLabel->setFont(QFont("Quicksand", 18, QFont::Bold));
    contentLayout->addWidget(headerLabel, 0, Qt::AlignLeft);
    // pad it out veritcally
    headerLabel->setContentsMargins(0, 16, 0, 12);
    return headerLabel;
  };

  // Add a label or any other widgets you want to display on the Settings screen
  auto addSettingsLabel = [contentLayout](QString name, QString desc, QWidget* action, std::function<void(QHBoxLayout*, QVBoxLayout*)> extra = nullptr) {
    auto layout = new QHBoxLayout();
    contentLayout->addLayout(layout);
    layout->setAlignment(Qt::AlignLeft);

    auto metadataLayout = new QVBoxLayout();
    layout->addLayout(metadataLayout);

    auto nameLabel = new QLabel(name); 
    nameLabel->setFont(QFont("Quicksand", 12, QFont::Bold)); 
    metadataLayout->addWidget(nameLabel); 

    auto descLabel = new QLabel(desc); 
    descLabel->setFont(QFont("Quicksand", 10)); 
    descLabel->setWordWrap(true); 
    descLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    metadataLayout->addWidget(descLabel); 

    layout->addWidget(action);

    if (extra) extra(layout, metadataLayout);

    return std::make_pair(nameLabel, descLabel);
  };


  auto settings = ProfileSettings::get();

  addSettingsHeader("Profile");

  auto* pfpChangeButton = new IconButton(LucideIcons::pen);

  addSettingsLabel(
    "Profile picture",
    "Change how your profile looks",
    pfpChangeButton,
    [this, settings, pfpChangeButton](QHBoxLayout* layout, QVBoxLayout* metadataLayout){
      auto* pfpLabel = new PixmapWidget;
      pfpLabel->setPixmap(QPixmap(settings->value(STK_PFP, ":/defaultuserprofile.png").toString()));
      pfpLabel->setFixedSize({64,64});
      layout->insertWidget(0, pfpLabel);

      connect(pfpChangeButton, &Button::clicked, this, [this, settings, pfpLabel](){
        auto pfpImagePath = QFileDialog::getOpenFileName(this, "Select a profile picture", "", "Images (*.png *.jpg *.jpeg *.bmp)");
        auto pfpStore = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()) / "profile_pictures";
        std::filesystem::create_directories(pfpStore);
        auto pfpFileName = std::filesystem::path(pfpImagePath.toStdString()).filename();
        auto pfpDestPath = pfpStore / pfpFileName;
        std::filesystem::copy_file(pfpImagePath.toStdString(), pfpDestPath, std::filesystem::copy_options::overwrite_existing);
        settings->setValue(STK_PFP, QString::fromStdString(pfpDestPath.string()));
        pfpLabel->setPixmap(QPixmap(settings->value(STK_PFP).toString()));
      });
    }
  );


  auto* displayNameTextInput = new QLineEdit();
  displayNameTextInput->setText(
    settings->value(STK_DISPLAYNAME, ProfileSingleApp::instance()->profileId()).toString()
  );
  bool haveDisplayName = settings->contains(STK_DISPLAYNAME);
  auto [dnNameLabel, dnDescLabel] = addSettingsLabel(
    "Display name", 
    haveDisplayName
      ? "Change the profile's display name."
      : "you dont want the name to look like that, do you?", 
    displayNameTextInput
  );
  connect(displayNameTextInput, &QLineEdit::editingFinished, this, [displayNameTextInput, dnDescLabel, haveDisplayName, settings](){
    settings->setValue(STK_DISPLAYNAME, displayNameTextInput->text());
    if (!haveDisplayName) 
      dnDescLabel->setText("great! :D");

    getMainWindow()->setPageTitleBar("Settings");
  });



  auto* pathChangeButton = new IconButton(LucideIcons::folder_pen, content);
  auto [pcNameLabel, pcDescLabel] = addSettingsLabel("Base game's path", settings->value(STK_BASEPATH).toString(), pathChangeButton);
  connect(pathChangeButton, &Button::clicked, this, [this, settings, pcDescLabel](){
    askForBasePathChange();

    pcDescLabel->setText(settings->value(STK_BASEPATH).toString());
  });


  /*
  auto* testButton1 = new Button("Dialog");
  connect(testButton1, &Button::clicked, this, [this](){
    Dialog::showActionDialog(
      getMainWindow(), 
      "Test Dialog", 
      "This is a test dialog.",
      "You can put any content here.",
      Dialog::DialogType::Confirm
    );
  });

  auto* testButton2 = new Button("Dialog with danger sfx");
  connect(testButton2, &Button::clicked, this, [this](){
    Dialog::showActionDialog(
      getMainWindow(), 
      "Test Dialog", 
      "This is a test dialog.",
      "You can put any content here.",
      Dialog::DialogType::Confirm,
      true
    );
  });

  contentLayout->addWidget(testButton1);
  contentLayout->addWidget(testButton2);
*/
#undef addSettingsLabel
}
