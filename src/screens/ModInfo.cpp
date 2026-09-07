#include "ModInfo.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"
#include "../ui/IconButton.hpp"
#include "../utils/LucideIcons.hpp"

ModInfoScreen::ModInfoScreen(QWidget* parent) : QWidget(parent) {
  // Set up the layout for the ModInfo screen
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);

  auto* header = new IconButton(LucideIcons::arrow_left);
  header->setToolTip("Back");
  connect(header, &IconButton::clicked, this, &ModInfoScreen::backButtonClicked);
  header->setFixedSize(32, 32);
  layout->addWidget(header);
  auto* content = new GradientBackground(this);
  layout->addWidget(content);
  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 8;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(0);

  // Add a label or any other widgets you want to display on the ModInfo screen
  auto* label = new QLabel("Welcome to the ModInfo Screen!", content);
  label->setAlignment(Qt::AlignCenter);
  contentLayout->addWidget(label);
}
