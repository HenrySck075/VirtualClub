#include "GlobalSettings.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QComboBox>
#include <QDateTime>
#include <string_view>

#include "../ui/GradientBackground.hpp"
#include "../ui/MESWidgets.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "ui/IconButton.hpp"
#include "ui/Router.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/AutoUpdater.hpp"

namespace {
bool moveStorageContents(const QString& sourcePath, const QString& destinationPath) {
  QDir source(sourcePath);
  QDir destination(destinationPath);

#define verify(condition,...) if (condition) {qDebug() << "moveStorageContents():"<<__VA_ARGS__; return false;}

  verify(!source.exists() || !destination.exists(), (source.exists() ? "destination" : "source") << "doesn't exist");

  const QString sourceCanonicalPath = QFileInfo(sourcePath).canonicalFilePath();
  const QString destinationCanonicalPath = QFileInfo(destinationPath).canonicalFilePath();

  verify(sourceCanonicalPath.isEmpty(), "source directory is somehow empty");
  verify(!destinationCanonicalPath.isEmpty(), "destination directory is not empty");
  verify(destinationCanonicalPath == sourceCanonicalPath, "source is the same as dest");
  verify(destinationCanonicalPath.startsWith(sourceCanonicalPath + QDir::separator()), "dest is a subfolder of source");
  verify(sourceCanonicalPath.startsWith(destinationCanonicalPath + QDir::separator()), "source is a subfolder of dest");

  const auto entries = source.entryInfoList(
      QDir::AllEntries | QDir::NoDotAndDotDot,
      QDir::DirsFirst | QDir::Name
  );

  for (const QFileInfo& entry : entries) {
    const QString destinationEntry = destination.filePath(entry.fileName());
    if (QFileInfo::exists(destinationEntry)) {
      return false;
    }
  }

  for (const QFileInfo& entry : entries) {
    const QString destinationEntry = destination.filePath(entry.fileName());
    if (!source.rename(entry.fileName(), destinationEntry)) {
      return false;
    }
  }

  return true;
}
}

GlobalSettingsScreen::GlobalSettingsScreen(Router *router, QWidget *parent) : QWidget(parent) {
  setWindowTitle("Global settings");

  auto *layout = new QVBoxLayout(this);
  constexpr int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(0);

  auto *content = new QWidget(this);
  setupGradientWidget(content);
  layout->addWidget(content);

  auto *contentLayout = new QVBoxLayout(content);
  constexpr int contentMargin = 16;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignTop);
  contentLayout->setHorizontalSizeConstraint(QLayout::SetMaximumSize);

  if (router) {
    auto *backButton = new IconButton(LucideIcons::arrow_left, content);
    contentLayout->addWidget(backButton, 0, Qt::AlignLeft);
    connect(backButton, &Button::clicked, this, [router]() {
      router->navigate("/settings");
    });
  }

  auto *notesLabel = new QLabel(tr("These settings apply to all profiles."), content);
  notesLabel->setStyleSheet("font-style: italic;");
  contentLayout->addWidget(notesLabel, 0, Qt::AlignLeft);

  auto *storageLabel = new QLabel(tr("Launcher storage path"), content);
  storageLabel->setFont(QFont("Quicksand", 12, QFont::Bold));
  contentLayout->addWidget(storageLabel);

  auto *storageDescription = new QLabel(
      tr("The directory used to store downloaded mods and launcher data."),
      content
  );
  storageDescription->setWordWrap(true);
  contentLayout->addWidget(storageDescription);

  auto *storageLayout = new QHBoxLayout();
  contentLayout->addLayout(storageLayout);

  auto *storagePath = new QLineEdit(content);
  storagePath->setText(QSettings().value(GSTK_STORAGE_PATH).toString());
  storagePath->setReadOnly(true);
  storageLayout->addWidget(storagePath);

  auto *browseButton = new Button(qApp->translate("Settings", "Change"), LucideIcons::folder_pen, content);
  storageLayout->addWidget(browseButton);

  connect(browseButton, &QPushButton::clicked, this, [this, storagePath]() {
    const QString currentPath = QSettings().value(GSTK_STORAGE_PATH).toString();
    const QString selectedPath = QFileDialog::getExistingDirectory(
        this,
        tr("Select launcher storage directory"),
        currentPath
    );

    if (selectedPath.isEmpty() || QDir::cleanPath(selectedPath) == QDir::cleanPath(currentPath)) {
      return;
    }

    const QString message = tr("Move all launcher data from \"%1\" to \"%2\"?")
                                .arg(currentPath, selectedPath);
    if (!Dialog::showActionDialog(
            getMainWindow(),
            tr("Confirm storage path"),
            message,
            tr("The existing storage directory will be emptied after its contents are moved."),
            Dialog::DialogType::YesNo,
            true
        )) {
      return;
    }

    if (!moveStorageContents(currentPath, selectedPath)) {
      Dialog::showActionDialog(
          getMainWindow(),
          tr("Storage path not changed"),
          tr("Could not move all launcher data to the selected directory."),
          tr("Choose an empty directory and try again."),
          Dialog::DialogType::Confirm,
          true
      );
      return;
    }

    QSettings settings;
    settings.setValue(GSTK_STORAGE_PATH, QDir::cleanPath(selectedPath));
    storagePath->setText(QDir::cleanPath(selectedPath));
  });

  auto* updatesLabel = new QLabel(tr("Updates"), content);
  updatesLabel->setFont(QFont("Quicksand", 12, QFont::Bold));
  contentLayout->addWidget(updatesLabel);

  auto* updateLayout = new QHBoxLayout();
  contentLayout->addLayout(updateLayout);

  auto* channelLabel = new QLabel(tr("Channel"), content);
  updateLayout->addWidget(channelLabel);
  auto* channel = new QComboBox(content);
  channel->addItem(tr("Stable"), "stable");
  channel->addItem(tr("Nightly"), "nightly");
  channel->setCurrentIndex(channel->findData(
      QSettings().value(GSTK_UPDATE_CHANNEL, QStringLiteral(MVC_CHANNEL)).toString()));
  updateLayout->addWidget(channel);

  auto* frequency = new QComboBox(content);
  frequency->addItem(tr("Every startup"), "startup");
  frequency->addItem(tr("Daily"), "daily");
  frequency->addItem(tr("Weekly"), "weekly");
  frequency->addItem(tr("Never"), "never");
  frequency->setCurrentIndex(frequency->findData(QSettings().value(GSTK_UPDATE_FREQUENCY, "daily").toString()));
  updateLayout->addWidget(frequency);


  auto* checkButton = new Button(tr("Check for updates"), LucideIcons::refresh_cw, content);
  contentLayout->addWidget(checkButton, 0, Qt::AlignLeft);

  
  if constexpr (std::string_view(MVC_CHANNEL) == "local") {
    channel->setEnabled(false);
    frequency->setEnabled(false);
    checkButton->setEnabled(false);
  } else {
    connect(channel, &QComboBox::currentIndexChanged, this, [channel](int index) {
      QSettings().setValue(GSTK_UPDATE_CHANNEL, channel->itemData(index).toString());
    });
    connect(frequency, &QComboBox::currentIndexChanged, this, [frequency](int index) {
      QSettings().setValue(GSTK_UPDATE_FREQUENCY, frequency->itemData(index).toString());
    });
    connect(checkButton, &QPushButton::clicked, this, [this]() {
      QSettings().setValue(GSTK_UPDATE_LAST_CHECK, QDateTime::currentDateTime());
      auto* updater = new AutoUpdater(this);
      connect(updater, &AutoUpdater::updateAvailable, this,
          [](const nlohmann::json& manifest) {
            if (auto* mainWindow = getMainWindow()) {
              mainWindow->showUpdateAvailable(manifest);
            }
          });
      connect(updater, &AutoUpdater::noUpdate, this, []() {
        Dialog::showActionDialog(getMainWindow(), tr("No updates"), tr("VirtualClub is up to date."), QString(), Dialog::Confirm, false);
      });
      connect(updater, &AutoUpdater::checkFailed, this, [](const QString& error) {
        Dialog::showActionDialog(getMainWindow(), tr("Update check failed"), error, QString(), Dialog::Confirm, false);
      });
      updater->checkForUpdate();
    });
  }
}
