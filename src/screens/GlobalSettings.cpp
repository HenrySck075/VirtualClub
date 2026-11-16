#include "GlobalSettings.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "../ui/GradientBackground.hpp"
#include "../ui/MESWidgets.hpp"
#include "consts.hpp"
#include "ui/IconButton.hpp"
#include "ui/Router.hpp"
#include "utils/LucideIcons.hpp"

namespace {
bool moveStorageContents(const QString& sourcePath, const QString& destinationPath) {
  QDir source(sourcePath);
  QDir destination(destinationPath);

  if (!source.exists() || !destination.exists()) {
    return false;
  }

  const QString sourceCanonicalPath = QFileInfo(sourcePath).canonicalFilePath();
  const QString destinationCanonicalPath = QFileInfo(destinationPath).canonicalFilePath();
  if (sourceCanonicalPath.isEmpty() || destinationCanonicalPath.isEmpty()
      || destinationCanonicalPath == sourceCanonicalPath
      || destinationCanonicalPath.startsWith(sourceCanonicalPath + QDir::separator())
      || sourceCanonicalPath.startsWith(destinationCanonicalPath + QDir::separator())) {
    return false;
  }

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

  auto *browseButton = new Button(tr("Change"), content);
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
            this,
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
          this,
          tr("Storage path not changed"),
          tr("Could not move all launcher data to the selected directory."),
          tr("Choose an empty directory on the same storage device and try again."),
          Dialog::DialogType::Confirm,
          true
      );
      return;
    }

    QSettings settings;
    settings.setValue(GSTK_STORAGE_PATH, QDir::cleanPath(selectedPath));
    storagePath->setText(QDir::cleanPath(selectedPath));
  });
}
