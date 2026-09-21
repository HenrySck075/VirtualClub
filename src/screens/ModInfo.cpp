#include "ModInfo.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"
#include "../ui/IconButton.hpp"
#include "../utils/LucideIcons.hpp"
#include "MainWindow.hpp"
#include "ui/MESWidgets.hpp"
#include "utils/ModIndex.hpp"
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

  auto* openModDirButton = new Button("Open mod directory", LucideIcons::folder);
  connect(openModDirButton, &Button::clicked, this, &ModInfoScreen::onOpenModDirClicked);
  headerLayout2->addWidget(openModDirButton, 0, Qt::AlignLeft);

  auto* content = new GradientBackground(this);
  content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  layout->addWidget(content);
  auto* contentLayout = new QHBoxLayout(content);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignLeft);

  m_playButton = new Button("Play", LucideIcons::play);
  m_playButton->setToolTip("Start the mod");
  connect(m_playButton, &Button::clicked, this, &ModInfoScreen::onPlayButtonClicked);
  contentLayout->addWidget(m_playButton); 

  m_playFromSaveButton = new Button("...from save", LucideIcons::rotate_cw_clock);
  m_playFromSaveButton->setToolTip("Start the mod from save file");
  contentLayout->addWidget(m_playFromSaveButton);

  m_deleteButton = new Button(LucideIcons::trash);
  m_deleteButton->setToolTip("Uninstall");
  connect(m_deleteButton, &Button::clicked, this, &ModInfoScreen::onDeleteButtonClicked);
  contentLayout->addWidget(m_deleteButton);

#ifndef MVC_VFS_AVAILABLE
  m_playButton->setEnabled(false);
  m_playButton->setToolTip("Playing is currently unsupported on this platform.");
  m_playFromSaveButton->setEnabled(false);
  m_playFromSaveButton->setToolTip("Playing is currently unsupported on this platform.");
#endif

  auto* content2 = new GradientBackground(this);
  content2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  layout->addWidget(content2);
  auto* contentLayout2 = new QVBoxLayout(content2);
  contentLayout2->setSpacing(8);
  contentLayout2->setAlignment(Qt::AlignTop);

  auto addThing = [contentLayout2](const QString& name, QWidget* actionWidget) {
    auto* tl = new QHBoxLayout();
    tl->setAlignment(Qt::AlignLeft);
    contentLayout2->addLayout(tl);

    tl->addWidget(actionWidget);
    tl->addWidget(new QLabel(name));
  };

  m_developerModeSwitch = new Switch();
  addThing("Developer Mode", m_developerModeSwitch);

  m_forceRecompileSwitch = new Switch();
  addThing("Force Recompile .rpyc", m_forceRecompileSwitch);
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
    m_developerModeSwitch->setEnabled(false);
    m_forceRecompileSwitch->setEnabled(false);

    setWindowTitle(QString::fromStdString(m_displayingMod->name)+" [Playing]");
    ModsIndex::Mod currentMod = m_displayingMod.value();
    SessionManager::launch(m_displayingMod.value(), [this,currentMod](){
      if (currentMod != m_displayingMod.value()) return;
      m_playButton->setEnabled(true);  
      m_playFromSaveButton->setEnabled(true);
      m_developerModeSwitch->setEnabled(true);
      m_forceRecompileSwitch->setEnabled(true);

      setWindowTitle(QString::fromStdString(m_displayingMod->name));
    });
  }
}

void ModInfoScreen::onDeleteButtonClicked() {
  if (Dialog::showActionDialog(
    getMainWindow(), 
    "Uninstall mod?", 
    "Are you sure want to uninstall the mod?",
    "This won't delete the mod's files, however it's save states and launcher configs will be removed.",
    Dialog::YesNo,
    true
  )) {
    ModsIndex::removeMod(m_displayingMod.value());
    emit modUninstalled(m_displayingMod->id); 
  }
}

void ModInfoScreen::setDisplayingMod(const ModsIndex::Mod& mod) {
  setWindowTitle(QString::fromStdString(mod.name));
  m_displayingMod = mod;
  QPixmap pixmap(QString::fromStdString(mod.getIconPath()));
  m_modIconLabel->setPixmap(pixmap);

  m_modNameLabel->setText(mod.name.c_str());
  m_modVersionLabel->setText(QString("Version: %1").arg(mod.version.c_str()));

  m_developerModeSwitch->setChecked(m_displayingMod->enableDeveloper);
  m_forceRecompileSwitch->setChecked(m_displayingMod->forceRecompile);

  if (SessionManager::isMounted(mod.id)) {
    m_playButton->setEnabled(false);
    m_playFromSaveButton->setEnabled(false);
    m_developerModeSwitch->setEnabled(false);
    m_forceRecompileSwitch->setEnabled(false);
  } else {
    m_playButton->setEnabled(true);
    m_playFromSaveButton->setEnabled(true);
    m_developerModeSwitch->setEnabled(true);
    m_forceRecompileSwitch->setEnabled(true);
  }

}
