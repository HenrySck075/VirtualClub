#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include "../ui/GradientBackground.hpp"
#include "../ui/MESWidgets.hpp"
#include "MainWindow.hpp"
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
  contentLayout->setSpacing(4);
  contentLayout->setAlignment(Qt::AlignTop);
  contentLayout->setHorizontalSizeConstraint(QLayout::SetMaximumSize);

  contentLayout->addWidget(new QLabel("<i>All changes are automatically saved.</i>"),0,Qt::AlignLeft);

  // Add a label or any other widgets you want to display on the Settings screen
  auto addSettingsLabel = [contentLayout](QString name, QString desc, QWidget* action) {
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

    return std::make_pair(nameLabel, descLabel);
  };


  auto settings = ProfileSettings::get();

  auto* pathChangeButton = new Button(LucideIcons::folder_pen, content);
  auto [pcNameLabel, pcDescLabel] = addSettingsLabel("Base game's path", settings->value("baseGameInstallPath").toString(), pathChangeButton);
  connect(pathChangeButton, &Button::clicked, this, [this, settings, pcDescLabel](){
    askForBasePathChange();

    pcDescLabel->setText(settings->value("baseGameInstallPath").toString());
  });

  auto* displayNameTextInput = new QLineEdit();
  displayNameTextInput->setText(settings->value("displayName", ProfileSingleApp::instance()->profileId()).toString());
  bool haveDisplayName = settings->contains("displayName");
  auto [dnNameLabel, dnDescLabel] = addSettingsLabel(
    "Display name", 
    haveDisplayName
      ? "Change the profile's display name."
      : "you dont want the name to look like that, do you?", 
    displayNameTextInput
  );
  connect(displayNameTextInput, &QLineEdit::editingFinished, this, [displayNameTextInput, dnDescLabel, haveDisplayName, settings](){
    settings->setValue("displayName", displayNameTextInput->text());
    if (!haveDisplayName) 
      dnDescLabel->setText("great! :D");

    getMainWindow()->setPageTitleBar("Settings");
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
