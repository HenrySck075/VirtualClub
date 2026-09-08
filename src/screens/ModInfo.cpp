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

  auto* navigation = new IconButton(LucideIcons::arrow_left);
  navigation->setToolTip("Back");
  connect(navigation, &IconButton::clicked, this, &ModInfoScreen::backButtonClicked);
  layout->addWidget(navigation);
  auto* content = new GradientBackground(this);
  layout->addWidget(content);
  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 8;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(4);
  contentLayout->setAlignment(Qt::AlignTop);

  auto* header = new QWidget();
  header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  contentLayout->addWidget(header);

  auto* headerLayout1 = new QHBoxLayout(header);
  headerLayout1->setSpacing(8);

  m_modIconLabel = new PixmapWidget();
  //m_modIconLabel->setPixmap(modIcon);
  headerLayout1->addWidget(m_modIconLabel);
  headerLayout1->setAlignment(Qt::AlignLeft);
  m_modIconLabel->setFixedSize({120,120});

  auto* metadataWidget = new QWidget();
  metadataWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  headerLayout1->addWidget(metadataWidget);

  auto* headerLayout2 = new QVBoxLayout(metadataWidget);
  headerLayout2->setSpacing(4);
  headerLayout2->setAlignment(Qt::AlignTop);
  
  m_modNameLabel = new QLabel();
  m_modNameLabel->setFont(QFont("Quicksand", 20, QFont::Weight::Bold));
  headerLayout2->addWidget(m_modNameLabel);

  m_modVersionLabel = new QLabel();
  m_modVersionLabel->setFont(QFont("Quicksand", 16));
  headerLayout2->addWidget(m_modVersionLabel);
}


void ModInfoScreen::setDisplayingMod(ModsIndex::Mod& mod) {
  QPixmap pixmap(mod.getIconPath().c_str());
  m_modIconLabel->setPixmap(pixmap);

  m_modNameLabel->setText(mod.name.c_str());
  m_modVersionLabel->setText(QString("Version: %1").arg(mod.version.c_str()));
}
