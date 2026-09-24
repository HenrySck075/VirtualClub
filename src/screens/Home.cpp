#include "Home.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"
#include "consts.hpp"
#include "utils/ProfileSettings.hpp"

HomeScreen::HomeScreen(QWidget* parent) : QWidget(parent) {
  // Set up the layout for the Home screen
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);

  auto* header = new QLabel(
    QString("wassup %1").arg(
      ProfileSettings::get()->value(STK_DISPLAYNAME).toString()
    ),
    this
  );
  header->setFont(QFont("Quicksand", 25, QFont::Weight::Bold));
  header->setStyleSheet("color: white;");
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  layout->addWidget(header);
  auto* content = new GradientBackground(this);
  layout->addWidget(content);

  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 16;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(0);
  contentLayout->setAlignment(Qt::AlignTop);

  // Add a label or any other widgets you want to display on the Home screen
  auto* label = new QLabel("Recently played", content);
  label->setFont(QFont("Quicksand", 18, QFont::Weight::Bold));
  contentLayout->addWidget(label);
}
