#include "Home.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"

HomeScreen::HomeScreen(QWidget* parent) : QWidget(parent) {
  setMinimumSize(QSize(200,200));
  // Set up the layout for the Home screen
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);

  auto* header = new QLabel("<font color='#FFFFFF'>hallo :3</font>",this);
  header->setFont(QFont("Quicksand", 30, QFont::Weight::Bold));
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  layout->addWidget(header);
  auto* content = new GradientBackground(this);
  layout->addWidget(content);
  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 8;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(0);

  // Add a label or any other widgets you want to display on the Home screen
  auto* label = new QLabel("Welcome to the Home Screen!", content);
  label->setAlignment(Qt::AlignCenter);
  contentLayout->addWidget(label);
}
