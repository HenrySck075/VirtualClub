#include "Settings.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"

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


  // Add a label or any other widgets you want to display on the Settings screen
  auto *label = new QLabel("m", content);
  label->setAlignment(Qt::AlignCenter);
  contentLayout->addWidget(label);
}
