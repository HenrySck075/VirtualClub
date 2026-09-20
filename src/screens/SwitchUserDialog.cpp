#include "SwitchUserDialog.hpp"
#include "MainWindow.hpp"
#include "ui/Sidebar.hpp" // for reused SidebarItem
#include "utils/ProfileSettings.hpp"

#include "ui/Dialog.hpp"

class SwitchUserDialogContent : public QWidget {
public:
  SwitchUserDialogContent() {
    // vbox
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);

    auto profileIds = ProfileSettings::list();
    
    for (const auto& profileId : profileIds) {
      auto s = ProfileSettings::getOf(profileId);
      QIcon icon(s->value("profileImage", ":/defaultuserprofile.png").toString());
      auto name = s->value("displayName", profileId).toString();
      auto* button = new SidebarItem(icon, name.toStdString(), false, this);
      button->setMaximumWidth(QWIDGETSIZE_MAX);
      button->setTintIcon(false);
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      connect(button, &SidebarItem::clicked, this, [profileId](){
        // launch a new instance with a new profile id
        //getMainWindow()->reload();
      });
      layout->addWidget(button);
    }
  }
};

void showSwitchUserDialog() {
  Dialog::showContentDialog(getMainWindow(), "Switch user", new SwitchUserDialogContent(), {300, 500});
}
