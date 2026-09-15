#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include "../ui/GradientBackground.hpp"
#include "../ui/Dialog.hpp"
#include "../MainWindow.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/utils.hpp"

SettingsScreen::SettingsScreen(QWidget *parent) : QWidget(parent) {
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
  contentLayout->setSpacing(2);
  contentLayout->setAlignment(Qt::AlignTop);
  contentLayout->setHorizontalSizeConstraint(QLayout::SetMaximumSize);

  // Add a label or any other widgets you want to display on the Settings screen
#define addSettingsLabel(prefix, name, desc)  \
  auto prefix##NameLabel = new QLabel(name, content); \
  prefix##NameLabel->setFont(QFont("Quicksand", 12, QFont::Bold)); \
  contentLayout->addWidget(prefix##NameLabel); \
\
  auto prefix##DescLabel = new QLabel(desc, content); \
  prefix##DescLabel->setFont(QFont("Quicksand", 10)); \
  prefix##DescLabel->setWordWrap(true); \
  contentLayout->addWidget(prefix##DescLabel); 


  auto settings = ProfileSettings::get();

  addSettingsLabel(pc, "Base game's path", settings->value("baseGameInstallPath").toString());
  auto* pathChangeButton = new Button("Change", content);
  connect(pathChangeButton, &Button::clicked, this, [this, settings, pcDescLabel](){
    askForBasePathChange();

    pcDescLabel->setText(settings->value("baseGameInstallPath").toString());
  });
  contentLayout->addWidget(pathChangeButton);

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
