#include "ModInfo.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/GradientBackground.hpp"
#include "../ui/IconButton.hpp"
#include "../utils/macros.h"
#include "../utils/LucideIcons.hpp"
#include "MainWindow.hpp"
#include "ui/PixmapWidget.hpp"
#include "ui/MESWidgets.hpp"
#include "utils/FlowLayout.hpp"
#include "utils/ModIndex.hpp"
#include "utils/SessionManager.hpp"
#include "utils/i18n.hpp"
#include "utils/utils.hpp"
#include <QDesktopServices>
#include <QPointer>
#include <QFileInfo>
#include <vector>
#include <third_party/kubazip/zip.h>
#include <nlohmann/json.hpp>

#include <QVariantAnimation>

class SSDCItem : public QWidget {
  Q_OBJECT
signals:
  void clicked();
private:
  QPointer<PixmapWidget> m_thumbnail;
  QString m_name;
  QString m_dateTimeStr;

  float m_hoverAnimationTimer = 0.0;
  QVariantAnimation* m_hoverAnimation;
public:
  explicit SSDCItem(PixmapWidget* thumbnail, const QString& name, const QString& dateTimeStr, QWidget* parent = nullptr)
    : QWidget(parent), m_thumbnail(thumbnail), m_name(name), m_dateTimeStr(dateTimeStr) {
      m_hoverAnimation = new QVariantAnimation(this);
      m_hoverAnimation->setStartValue(0.0);
      m_hoverAnimation->setEndValue(1.0);
      m_hoverAnimation->setDuration(100);

      connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_hoverAnimationTimer = value.toFloat();
        update();
      });

      auto* layout = new QVBoxLayout(this);
      layout->setContentsMargins(8,8,8,8);
      layout->setSpacing(2);
      layout->setAlignment(Qt::AlignTop);
      m_thumbnail->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
      layout->addWidget(m_thumbnail);
      auto* nameLabel = new QLabel(name);
      nameLabel->setFont(QFont("Quicksand", 10, QFont::DemiBold));
      nameLabel->setAlignment(Qt::AlignCenter);
      layout->addWidget(nameLabel);
      auto* dateTimeLabel = new QLabel(dateTimeStr);
      dateTimeLabel->setFont(QFont("Quicksand", 9));
      dateTimeLabel->setAlignment(Qt::AlignCenter);
      layout->addWidget(dateTimeLabel);

      // lazy bum
      for (QWidget *child : findChildren<QWidget*>()) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
      }
    };
protected:
  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      qDebug() << "balls";
      emit clicked();
    }

    QWidget::mouseReleaseEvent(event);
  }
  void enterEvent(QEnterEvent* event) override {
    m_hoverAnimation->setDirection(QAbstractAnimation::Forward);
    m_hoverAnimation->start();
    QWidget::enterEvent(event);
  }
  void leaveEvent(QEvent* event) override {
    m_hoverAnimation->setDirection(QAbstractAnimation::Backward);
    m_hoverAnimation->start();
    QWidget::leaveEvent(event);
  }

  // only need to paint a pink background.
  // except this rectangle is 8px bigger than the widget
  void paintEvent(QPaintEvent* event) override {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_hoverAnimationTimer > 0.0) {
      QColor hoverColor = QColor(241, 126, 187, static_cast<int>(m_hoverAnimationTimer * 100));
      painter.fillRect(rect(), hoverColor);
    }
  }
};

class ModInfoScreen;
class SaveSelectDialogContent : public DialogContent {
  Q_OBJECT
  ModsIndex::Mod m_mod;

  QPointer<ModInfoScreen> m_modInfoScreen;
public:
  // Get the folder where Ren'Py store every game's save data.
  static std::filesystem::path getRenpySaveDirectory() {
#if 0
#ifdef MVC_IS_WINDOWNS
    return std::filesystem::path(std::getenv("APPDATA")) / "RenPy";
#elif defined(MVC_IS_MAC)
    return std::filesystem::path(std::getenv("HOME")) / "Library" / "Application Support" / "RenPy";
#else
    return std::filesystem::path(std::getenv("HOME")) / ".renpy";
#endif
#endif
    return std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()) / "saves";
  }
  explicit SaveSelectDialogContent(const ModsIndex::Mod& mod, ModInfoScreen* mic) : m_mod(mod), m_modInfoScreen(mic) {
    auto saveDir = getRenpySaveDirectory() / mod.id;
    auto fallbackSaveDir = std::filesystem::path(mod.modPath) / "game" / "saves";

    // collect every "\d-\d-LT1.save" files in both folders (sorted)
    std::vector<std::filesystem::path> saveFiles;
    if (std::filesystem::exists(saveDir)) {
      for (const auto& entry : std::filesystem::directory_iterator(saveDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".save") {
          saveFiles.push_back(entry.path());
        }
      }
    }
    if (std::filesystem::exists(fallbackSaveDir)) {
      for (const auto& entry : std::filesystem::directory_iterator(fallbackSaveDir)) {
        // same check, plus if the exact same filename is already in saveFiles
        // (remember, entry.path() returns a whole path)
        if (entry.is_regular_file() && entry.path().extension() == ".save" &&
            std::find_if(saveFiles.begin(), saveFiles.end(), [&](const std::filesystem::path& p) {
              return p.filename() == entry.path().filename();
            }) == saveFiles.end()) {
          saveFiles.push_back(entry.path());
        }
      }
    }

    if (saveFiles.empty()) {
      auto* label = new QLabel(tr("No saves found. Remember to create some on your playthroughs, it will be helpful :)"));
      label->setAlignment(Qt::AlignCenter);
      label->setWordWrap(true);
      auto* layout = new QVBoxLayout(this);
      layout->addWidget(label);
    } else {
      auto* layout = new FlowLayout(this, 4);


      for (const auto& savePath : saveFiles) {
        auto zip_filename = savePath.string();

        auto* zip_archive = zip_open(zip_filename.c_str(), 0, 'r'); 

        auto* thumbnailWidget = new PixmapWidget();
        thumbnailWidget->setFixedSize({256, 144});

        std::string saveName = savePath.filename().string();
        {
          void* buffer;
          size_t bufsize;

          zip_entry_open(zip_archive, "screenshot.png");
          zip_entry_read(zip_archive, &buffer, &bufsize);
          zip_entry_close(zip_archive);
          QPixmap pixmap;
          if (pixmap.loadFromData((unsigned char*)buffer, static_cast<int>(bufsize))) {
            // renpy save screenshots is 256x144 i checked
            thumbnailWidget->setPixmap(pixmap);
          }
        }

        {
          void* buffer;
          size_t bufsize;

          // Get additional data just in case
          // we just need to load this file (as json) for ["_save_name"]
          zip_entry_open(zip_archive, "json");
          zip_entry_read(zip_archive, &buffer, &bufsize);
          zip_entry_close(zip_archive);

          try {
            std::vector<unsigned char> bufferVec((unsigned char*)buffer, (unsigned char*)buffer + bufsize);
            auto jsonData = nlohmann::json::parse(bufferVec);
            if (jsonData.contains("_save_name") && jsonData["_save_name"].is_string()) {
              auto maybeSaveName = jsonData["_save_name"].get<std::string>();
              if (!maybeSaveName.empty()) 
                saveName = maybeSaveName;

            }
          } catch (const std::exception& e) {
            qDebug() << "Failed to parse JSON metadata in save file" << QString::fromStdString(zip_filename) << "because:" << e.what();
          }
        }

        zip_close(zip_archive);


        QFileInfo fileInfo(QString::fromStdString(savePath.string()));
        QString dateTimeStr = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");

        // saveId is the (numerical) part around the first dash of the save filename
        const std::string suffix = "-LT1.save"; // presumably unchanged 
        auto saveFilename = savePath.filename().string();
        auto saveId = saveFilename.substr(0, saveFilename.size() - suffix.length());

        auto* item = new SSDCItem(thumbnailWidget, QString::fromStdString(saveName), dateTimeStr);
        connect(item, &SSDCItem::clicked, this, [this, saveId](){
          if (m_modInfoScreen) {
            m_modInfoScreen->play(QString::fromStdString(saveId));
            closeDialog();
          }
        });
        layout->addWidget(item);
      }
    }
  };
};

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

  auto* headerLayout = new QVBoxLayout(header);

  auto* headerLayout1 = new QHBoxLayout();
  headerLayout1->setSpacing(8);
  headerLayout->addLayout(headerLayout1);

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
  $setDynamicLabelTextAndRegisterAutoTL(m_modVersionLabel, "Version: %1");
  headerLayout2->addWidget(m_modVersionLabel);

  auto* openModDirButton = new Button(LucideIcons::folder);
  $setLabelTextAndRegisterAutoTL(openModDirButton, "Open mod directory");
  connect(openModDirButton, &Button::clicked, this, &ModInfoScreen::onOpenModDirClicked);
  headerLayout2->addWidget(openModDirButton, 0, Qt::AlignLeft);

  // you cant play on such platforms anyway
#ifdef MVC_VFS_AVAILABLE
  m_playtimeLabel = new QLabel(); 
  m_playtimeLabel->setStyleSheet("color: #000000;");

  // TODO:
  headerLayout->addWidget(m_playtimeLabel);

  updatePlaytimeLabel();
#endif

  auto* content = new GradientBackground(this);
  content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  layout->addWidget(content);
  auto* contentLayout = new QHBoxLayout(content);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignLeft);

  m_playButton = new Button(LucideIcons::play);
  addRetranslateCallback([this](){
    m_playButton->setText(tr("Play"));
  }); 
  connect(m_playButton, &Button::clicked, this, &ModInfoScreen::onPlayButtonClicked);
  contentLayout->addWidget(m_playButton); 

  m_playFromSaveButton = new Button(LucideIcons::rotate_cw_clock);
  addRetranslateCallback([this](){
    m_playFromSaveButton->setText(tr("...from save"));
    m_playFromSaveButton->setToolTip(tr("Start the mod from save file"));
  });
  connect(m_playFromSaveButton, &Button::clicked, [this](){
    Dialog::showContentDialog(getMainWindow(), tr("Select save"), new SaveSelectDialogContent(m_displayingMod.value(), this), {900,600});
  });
  contentLayout->addWidget(m_playFromSaveButton);

  m_deleteButton = new IconButton(LucideIcons::trash);
  connect(m_deleteButton, &IconButton::clicked, this, &ModInfoScreen::onDeleteButtonClicked);
  contentLayout->addWidget(m_deleteButton);

  auto openMountDirButton = new IconButton(LucideIcons::folder_open_dot);
  connect(openMountDirButton, &IconButton::clicked, this, [this](){
    if (m_displayingMod.has_value()) {
      SessionManager::mount(m_displayingMod.value());
      auto path = SessionManager::mountPathOf(m_displayingMod->id);
      // open the directory
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
  });
  openMountDirButton->setToolTip("Open the mod's mount directory (where the mod is mounted to be used by Ren'Py)");
  contentLayout->addWidget(openMountDirButton);

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

  auto addThing = [this, contentLayout2](const QString& name, QWidget* actionWidget) {
    auto* tl = new QHBoxLayout();
    tl->setAlignment(Qt::AlignLeft);
    contentLayout2->addLayout(tl);

    tl->addWidget(actionWidget);

    auto* label = new QLabel();
    $setLabelTextAndRegisterAutoTL(label, name.toStdString().c_str());
    tl->addWidget(label);
  };

  m_developerModeSwitch = new Switch();
  addThing(trNoop("Developer Mode"), m_developerModeSwitch);

  m_forceRecompileSwitch = new Switch();
  addThing(trNoop("Force Recompile .rpyc"), m_forceRecompileSwitch);
}

void ModInfoScreen::updatePlaytimeLabel() {
  if (m_displayingMod.has_value() && m_playtimeLabel) {
    auto playtime = m_displayingMod->getPlaytime();
    if (playtime.has_value()) {
      auto [playtimeRaw, playtimeActive] = playtime.value();
      // use the active time for display
      m_playtimeLabel->setText(
        QString("Playtime: %1").arg(format_duration(playtimeActive))
      );
    } else {
      m_playtimeLabel->setText("Not played yet!");
    }
  }
}

void ModInfoScreen::onOpenModDirClicked() {
  /*
  if (m_displayingMod.has_value()) {
    SessionManager::mount(m_displayingMod.value());
    auto path = SessionManager::mountPathOf(m_displayingMod->id);
    // open the directory
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }*/ 
  if (m_displayingMod.has_value()) {
    QDesktopServices::openUrl(QString::fromStdString(m_displayingMod->modPath));
  }
}

void ModInfoScreen::onPlayButtonClicked() {
  play();
}
void ModInfoScreen::play(const QString& saveId) {
#ifdef MVC_VFS_AVAILABLE
  if (m_displayingMod.has_value()) {
    ModsIndex::Mod currentMod = m_displayingMod.value();
    SessionManager::launch(m_displayingMod.value(), [this](){
      m_playButton->setEnabled(false);
      m_playFromSaveButton->setEnabled(false);
      m_developerModeSwitch->setEnabled(false);
      m_forceRecompileSwitch->setEnabled(false);

      setWindowTitle(QString::fromStdString(m_displayingMod->name)+" [Playing]");
    }, [this,currentMod](){
      if (currentMod != m_displayingMod.value()) return;
      m_playButton->setEnabled(true);  
      m_playFromSaveButton->setEnabled(true);
      m_developerModeSwitch->setEnabled(true);
      m_forceRecompileSwitch->setEnabled(true);

      setWindowTitle(QString::fromStdString(m_displayingMod->name));

      updatePlaytimeLabel();
    }, saveId);
  }
#endif
}

void ModInfoScreen::onDeleteButtonClicked() {
  if (Dialog::showActionDialog(
    getMainWindow(), 
    tr("Uninstall mod?"), 
    tr("Are you sure want to uninstall the mod?"),
    tr("This won't delete the mod's files, however it's save states and launcher configs will be removed."),
    Dialog::YesNo,
    true
  )) {
    ModsIndex::get()->removeMod(m_displayingMod.value());
    emit modUninstalled(m_displayingMod->id); 
  }
}

void ModInfoScreen::setDisplayingMod(const ModsIndex::Mod& mod) {
  bool isPlaying = SessionManager::isPlaying(mod.id);
  setWindowTitle(QString::fromStdString(mod.name) + (isPlaying ? " [Playing]" : ""));
  m_displayingMod = mod;
  QPixmap pixmap(QString::fromStdString(mod.getIconPath()));
  m_modIconLabel->setPixmap(pixmap);

  m_modNameLabel->setText(mod.name.c_str());
  $changeLabelPlaceholderArgs(m_modVersionLabel, QString::fromStdString(mod.version));

  m_developerModeSwitch->setChecked(m_displayingMod->enableDeveloper);
  m_forceRecompileSwitch->setChecked(m_displayingMod->forceRecompile);

#ifdef MVC_VFS_AVAILABLE
  if (isPlaying) {
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

  updatePlaytimeLabel();
#endif
}

#include "ModInfo.moc"
