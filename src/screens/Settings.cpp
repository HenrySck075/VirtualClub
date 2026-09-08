#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include "../ui/GradientBackground.hpp"
#include "../ui/Dialog.hpp"
#include "../MainWindow.hpp"

SettingsScreen::SettingsScreen(QWidget *parent) : QWidget(parent) {
  // Set up the layout for the Settings screen
  auto *layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(0);

  auto* content = new GradientBackground(this);
  layout->addWidget(content);
  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 8;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(0);

  auto* lineEdit = new QLineEdit();
  lineEdit->setPlaceholderText("Path");
  contentLayout->addWidget(lineEdit);

  // Add a label or any other widgets you want to display on the Settings screen
  auto *label = new QLabel("m", content);
  label->setAlignment(Qt::AlignCenter);
  contentLayout->addWidget(label);

  auto* testButton1 = new Button("Dialog");
  connect(testButton1, &Button::clicked, this, [this](){
    Dialog::showDialog(
      getMainWindow(), 
      "Test Dialog", 
      "This is a test dialog.",
      "You can put any content here.",
      Dialog::DialogType::Confirm
    );
  });

  auto* testButton2 = new Button("Dialog with danger sfx");
  connect(testButton2, &Button::clicked, this, [this](){
    Dialog::showDialog(
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
}
