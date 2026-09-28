#include "UserSetupDialog.hpp"
#include "ui/MESWidgets.hpp"
#include "ui/PixmapWidget.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "consts.hpp"
#include "utils/utils.hpp"
#include <QLineEdit>

class UserSetupDialogContent : public DialogContent {
public:
  UserSetupDialogContent() {
    auto s = ProfileSettings::get();

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* profilePic = new PixmapWidget(this);
    profilePic->setPixmap(QPixmap(s->value(STK_PFP, ":/defaultuserprofile.png").toString()).scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    profilePic->setStyleSheet(QString("border-radius: 50px"));
    profilePic->setFixedSize({100,100});
    layout->addWidget(profilePic, 0, Qt::AlignHCenter);

    auto* nameEdit = new QLineEdit(s->value(STK_DISPLAYNAME, ProfileSingleApp::instance()->profileName()).toString(), this);
    nameEdit->setFont(QFont("Quicksand", 12, QFont::Bold));
    nameEdit->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameEdit);

    auto* basePathLabel = new QLabel("Base game's path (Required)", this);
    basePathLabel->setFont(QFont("Quicksand", 10, QFont::Bold));
    layout->addWidget(basePathLabel);

    auto* basePathText = new QLabel(s->value(STK_BASEPATH).toString(), this);
    basePathText->setWordWrap(true);
    layout->addWidget(basePathText);

    auto* changeButton = new Button("Change", LucideIcons::folder, this);
    layout->addWidget(changeButton, 0, Qt::AlignHCenter);
    connect(changeButton, &QPushButton::clicked, this, [basePathText,s]() {
      askForBasePathChange();
      basePathText->setText(s->value(STK_BASEPATH).toString());
    });
  }
  bool allowClosing() override {
    return ProfileSettings::get()->contains(STK_BASEPATH);
  }
};

void showUserSetupDialogIfNeeded(bool force) {
  auto s = ProfileSettings::get();

  if (!force) {
    if (s->contains(STK_BASEPATH)) return;
  }

  Dialog::showContentDialog(nullptr, "Setup user", new UserSetupDialogContent(), {451, 600});
}

