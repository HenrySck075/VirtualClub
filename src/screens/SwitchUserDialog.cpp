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

    auto profileIds = ProfileSettings::list();
    
    for (const auto& profileId : profileIds) {
      auto* button = new Button(profileId, this);
      connect(button, &Button::clicked, this, [profileId](){
        ProfileSettings::setActiveProfile(profileId);
        //getMainWindow()->reload();
      });
      layout->addWidget(button);
    }
  }
};

void showSwitchUserDialog() {
  Dialog::showContentDialog(getMainWindow(), "Switch user", new SwitchUserDialogContent(), {300, 500});
}
