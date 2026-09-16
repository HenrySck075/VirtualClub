#include "ModInfo.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"
#include "../ui/IconButton.hpp"
#include "../utils/LucideIcons.hpp"
#include "ui/Dialog.hpp"
#include "utils/SessionManager.hpp"
#include <QDesktopServices>

ModInfoScreen::ModInfoScreen(QWidget* parent) : QWidget(parent) {
  // Set up the layout for the ModInfo screen
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);
  layout->setAlignment(Qt::AlignTop);

  auto* navigation = new IconButton(LucideIcons::arrow_left);
  navigation->setToolTip("Back");
  connect(navigation, &IconButton::clicked, this, &ModInfoScreen::backButtonClicked);
  layout->addWidget(navigation);
  auto* header = new GradientBackground(this);
  layout->addWidget(header);
  static const int contentMargin = 16;

  auto* headerLayout1 = new QHBoxLayout(header);
  headerLayout1->setSpacing(8);

  m_modIconLabel = new PixmapWidget();
  //m_modIconLabel->setPixmap(modIcon);
  headerLayout1->addWidget(m_modIconLabel);
  headerLayout1->setAlignment(Qt::AlignLeft);
  m_modIconLabel->setFixedSize({120,120});

  auto* headerLayout2 = new QVBoxLayout();
  headerLayout2->setSpacing(4);
  headerLayout2->setAlignment(Qt::AlignTop);
  headerLayout1->addLayout(headerLayout2);
  
  m_modNameLabel = new QLabel();
  m_modNameLabel->setFont(QFont("Quicksand", 20, QFont::Weight::Bold));
  headerLayout2->addWidget(m_modNameLabel);

  m_modVersionLabel = new QLabel();
  m_modVersionLabel->setFont(QFont("Quicksand", 16));
  headerLayout2->addWidget(m_modVersionLabel);

  auto* openModDirButton = new Button("Open mod directory");
  connect(openModDirButton, &Button::clicked, this, &ModInfoScreen::onOpenModDirClicked);
  headerLayout2->addWidget(openModDirButton, 0, Qt::AlignLeft);

  auto* content = new GradientBackground(this);
  content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  layout->addWidget(content);
  auto* contentLayout = new QHBoxLayout(content);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignLeft);

  m_playButton = new Button("Play", LucideIcons::play);
  connect(m_playButton, &Button::clicked, this, &ModInfoScreen::onPlayButtonClicked);
  contentLayout->addWidget(m_playButton); 

  m_playFromSaveButton = new Button("...from save", LucideIcons::rotate_cw_clock);
  contentLayout->addWidget(m_playFromSaveButton);

#ifndef MVC_VFS_AVAILABLE
  m_playButton->setEnabled(false);
  m_playButton->setToolTip("Playing is currently unsupported on this platform.");
  m_playFromSaveButton->setEnabled(false);
  m_playFromSaveButton->setToolTip("Playing is currently unsupported on this platform.");
#endif
}

void ModInfoScreen::onOpenModDirClicked() {
  if (m_displayingMod.has_value()) {
    SessionManager::mount(m_displayingMod.value());
    auto path = SessionManager::mountPathOf(m_displayingMod->id);
    // open the directory
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }
}

void ModInfoScreen::onPlayButtonClicked() {
  if (m_displayingMod.has_value()) {
    m_playButton->setEnabled(false);
    m_playFromSaveButton->setEnabled(false);
    SessionManager::launch(m_displayingMod.value(), [this](){
      m_playButton->setEnabled(true);  
      m_playFromSaveButton->setEnabled(true);
    });
  }
}

void ModInfoScreen::setDisplayingMod(const ModsIndex::Mod& mod) {
  m_displayingMod = mod;
  QPixmap pixmap(QString::fromStdString(mod.getIconPath()));
  m_modIconLabel->setPixmap(pixmap);

  m_modNameLabel->setText(mod.name.c_str());
  m_modVersionLabel->setText(QString("Version: %1").arg(mod.version.c_str()));
}
