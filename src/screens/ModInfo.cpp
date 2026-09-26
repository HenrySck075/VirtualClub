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
#include <QDesktopServices>
#include <QPointer>
#include <QFileInfo>
#include <vector>
#include <third_party/miniz/miniz.h>
#include <nlohmann/json.hpp>

#include <QVariantAnimation>

class SSDCItem : public QWidget {
  Q_OBJECT;
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
  Q_OBJECT;
  ModsIndex::Mod m_mod;

  QPointer<ModInfoScreen> m_modInfoScreen;
public:
  // Get the folder where Ren'Py store every game's save data.
  static std::filesystem::path getRenpySaveDirectory() {
#ifdef MVC_IS_WINDOWNS
    return std::filesystem::path(std::getenv("APPDATA")) / "RenPy";
#elif defined(MVC_IS_MAC)
    return std::filesystem::path(std::getenv("HOME")) / "Library" / "Application Support" / "RenPy";
#else
    return std::filesystem::path(std::getenv("HOME")) / ".renpy";
#endif
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

    auto* layout = new FlowLayout(this, 4);


    for (const auto& savePath : saveFiles) {
      auto zip_filename = savePath.string();

      mz_zip_archive zip_archive;
      memset(&zip_archive, 0, sizeof(zip_archive));

      if (!mz_zip_reader_init_file(&zip_archive, zip_filename.c_str(), 0)) {
        qDebug() << "Failed to open ZIP file:" << zip_filename;
        mz_zip_reader_end(&zip_archive);
        continue;
      }

      int num_files = mz_zip_reader_get_num_files(&zip_archive);

      auto* thumbnailWidget = new PixmapWidget();
      thumbnailWidget->setFixedSize({256, 144});

      std::string saveName = savePath.filename().string();

      bool saveSSDone = false;
      bool metadataDone = false;

      for (int i = 0; i < num_files; ++i) {
        if (saveSSDone && metadataDone) {
          break;
        } 

        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
          qDebug() << "Failed to get file stat for file index" << i << "in ZIP file:" << zip_filename;
          continue;
        }


#define mz_zip_reader_is_file_exists(filename) strcmp(file_stat.m_filename, filename) == 0 && !mz_zip_reader_is_file_a_directory(&zip_archive, i)
        // Get screenshot image
        if (!saveSSDone && mz_zip_reader_is_file_exists("screenshot.png")) {
          size_t uncompressed_size = static_cast<size_t>(file_stat.m_uncomp_size);
          std::vector<unsigned char> buffer(uncompressed_size);

          if (mz_zip_reader_extract_to_mem(&zip_archive, i, buffer.data(), uncompressed_size, 0)) {
            QPixmap pixmap;
            if (pixmap.loadFromData(buffer.data(), static_cast<int>(uncompressed_size))) {
              // renpy save screenshots is 256x144 i checked
              thumbnailWidget->setPixmap(pixmap);
            }
          }
          saveSSDone = true;
          continue;
        }

        // Get additional data just in case
        // we just need to load this file (as json) for ["_save_name"]
        if (!metadataDone && mz_zip_reader_is_file_exists("json")) {
          size_t uncompressed_size = static_cast<size_t>(file_stat.m_uncomp_size);
          std::vector<unsigned char> buffer(uncompressed_size);

          if (mz_zip_reader_extract_to_mem(&zip_archive, i, buffer.data(), uncompressed_size, 0)) {
            try {
              auto jsonData = nlohmann::json::parse(buffer);
              if (jsonData.contains("_save_name") && jsonData["_save_name"].is_string()) {
                auto maybeSaveName = jsonData["_save_name"].get<std::string>();
                if (!maybeSaveName.empty()) 
                  saveName = maybeSaveName;

              }
            } catch (const std::exception& e) {
              qDebug() << "Failed to parse JSON metadata in save file" << QString::fromStdString(zip_filename) << "because:" << e.what();
            }
          }
          metadataDone = true;
          continue;
        }
      }

      mz_zip_reader_end(&zip_archive);

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
  connect(m_playFromSaveButton, &Button::clicked, [this](){
    Dialog::showContentDialog(getMainWindow(), "Select save", new SaveSelectDialogContent(m_displayingMod.value(), this), {900,600});
  });
  contentLayout->addWidget(m_playFromSaveButton);

  m_deleteButton = new Button("Uninstall", LucideIcons::trash);
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
  play();
}
void ModInfoScreen::play(const QString& saveId) {
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
    }, saveId);
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
  bool isPlaying = SessionManager::isPlaying(mod.id);
  setWindowTitle(QString::fromStdString(mod.name) + (isPlaying ? " [Playing]" : ""));
  m_displayingMod = mod;
  QPixmap pixmap(QString::fromStdString(mod.getIconPath()));
  m_modIconLabel->setPixmap(pixmap);

  m_modNameLabel->setText(mod.name.c_str());
  m_modVersionLabel->setText(QString("Version: %1").arg(mod.version.c_str()));

  m_developerModeSwitch->setChecked(m_displayingMod->enableDeveloper);
  m_forceRecompileSwitch->setChecked(m_displayingMod->forceRecompile);

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

}

#include "ModInfo.moc"
