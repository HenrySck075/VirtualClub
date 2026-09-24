#include "SwitchUserDialog.hpp"
#include "MainWindow.hpp"
#include "ui/IconButton.hpp"
#include "ui/Sidebar.hpp" // for reused SidebarItem
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"

#include "ui/MESWidgets.hpp"
#include <qframe.h>

class SwitchUserDialogContent : public DialogContent {
public:


SwitchUserDialogContent() {
  auto* layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignTop);

  auto* incompleteLabel = new QLabel("<i>incomplete feature do not use pls thx</i>");
  incompleteLabel->setFont(QFont("Quicksand", 9));
  layout->addWidget(incompleteLabel);

  auto* userList = new QScrollArea(this);
  userList->setWidgetResizable(true);
  userList->setFrameShape(QFrame::NoFrame);

  // 1. Make QScrollArea and its internal viewport background transparent or explicitly auto-filled
  userList->setStyleSheet("QScrollArea { background: transparent; }");
  userList->viewport()->setStyleSheet("background: transparent;");

  auto* container = new QWidget();
  // 2. Enable auto-fill on the container so Qt explicitly paints its background palette
  container->setAutoFillBackground(true);
  
  // Alternatively, if you want a transparent scroll list, use:
  // container->setAttribute(Qt::WA_TranslucentBackground);

  auto* userListLayout = new QVBoxLayout(container);
  userListLayout->setAlignment(Qt::AlignTop);

  auto profileIds = ProfileSettings::list();
  
  for (const auto& profileId : profileIds) {
    auto s = ProfileSettings::getOf(profileId);
    QIcon icon(s->value("profileImage", ":/defaultuserprofile.png").toString());
    auto name = s->value("displayName", profileId).toString();
    
    auto* button = new SidebarItem(icon, name.toStdString(), false, container);
    button->setMaximumWidth(QWIDGETSIZE_MAX);
    button->setTintIcon(false);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    connect(button, &SidebarItem::clicked, this, [profileId](){
      // launch a new instance with a new profile id
    });
    
    userListLayout->addWidget(button);
  }

  userList->setWidget(container);
  layout->addWidget(userList);

  layout->addSpacing(1);

  auto* addProfileButton = new IconButton(LucideIcons::plus);
  layout->addWidget(addProfileButton, 0, Qt::AlignRight);
}


};

void showSwitchUserDialog() {
  Dialog::showContentDialog(getMainWindow(), "Switch user", new SwitchUserDialogContent(), {300, 500});
}
