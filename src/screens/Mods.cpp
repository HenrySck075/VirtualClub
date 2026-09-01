#include "Mods.hpp"
#include <QLayout>
#include <QLabel>

ModsScreen::ModsScreen(QWidget *parent) : QWidget(parent) {
  // Set up the layout for the Mods screen
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Add a label or any other widgets you want to display on the Mods screen
  auto *label = new QLabel("Welcome to the Mods Screen!", this);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label);
}
