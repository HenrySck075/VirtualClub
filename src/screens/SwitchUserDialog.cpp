#include "SwitchUserDialog.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "ui/IconButton.hpp"
#include "ui/Sidebar.hpp" // for reused SidebarItem
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"

#include "ui/MESWidgets.hpp"
#include "ui/PixmapWidget.hpp"
#include "utils/utils.hpp"
#include <QFrame>
#include <QProcess>

class UserListItem : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(int hoverAlpha READ hoverAlpha WRITE setHoverAlpha)

signals:
    void selectedChanged();

private:    
    int m_hoverAnimationValue = 0; // Ranges from 0 to 255
    QPropertyAnimation *m_fadeAnimation = nullptr;

    PixmapWidget *m_iconWidget = nullptr;
    QLabel *m_labelWidget = nullptr;
    QPushButton *m_deleteButton = nullptr;

    bool m_selected = false;
    bool m_hovered = false;
    bool m_switchable = true;

    int hoverAlpha() const { return m_hoverAnimationValue; }
    void setHoverAlpha(int alpha) {
        if (m_hoverAnimationValue != alpha) {
            m_hoverAnimationValue = alpha;
            updateStyleSheet();
        }
    }

    void updateStyleSheet() {
        // Build background gradient dynamically based on m_selected and m_hoverAnimationValue
        QString bgStyle;
        if (m_selected) {
            bgStyle = QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);")
                      .arg(c_secondaryColor.name(), c_primaryColor.name());
        } else if (m_hoverAnimationValue > 0) {
            QColor startColor = c_primaryLightColor;
            QColor stopColor = c_secondaryLightColor;
            startColor.setAlpha(m_hoverAnimationValue);
            stopColor.setAlpha(m_hoverAnimationValue);

            bgStyle = QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);")
                      .arg(startColor.name(QColor::HexArgb), stopColor.name(QColor::HexArgb));
        } else {
            bgStyle = "background: transparent;";
        }

        setStyleSheet(QString("UserListItem { %1 border: none; }").arg(bgStyle));

        // Update foreground colors
        QColor activeTextColor = (m_selected || m_hovered) ? QColorConstants::White : c_primaryColor;
        m_labelWidget->setStyleSheet(QString("color: %1; font-family: 'Quicksand'; font-size: 12pt; font-weight: 600; background: transparent;")
                                     .arg(activeTextColor.name()));
    }

protected:
    void enterEvent(QEnterEvent *event) override {
        Q_UNUSED(event);
        m_hovered = true;
        updateStyleSheet();
        m_fadeAnimation->stop();
        m_fadeAnimation->setStartValue(m_hoverAnimationValue);
        m_fadeAnimation->setEndValue(255);
        m_fadeAnimation->start();

        m_deleteButton->show();
    }

    void leaveEvent(QEvent *event) override {
        Q_UNUSED(event);
        m_hovered = false;
        updateStyleSheet();
        m_fadeAnimation->stop();
        m_fadeAnimation->setStartValue(m_hoverAnimationValue);
        m_fadeAnimation->setEndValue(0);
        m_fadeAnimation->start();

        m_deleteButton->hide();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (m_switchable) {
                setSelected(true);
                emit clicked();
            }
        }
        QPushButton::mouseReleaseEvent(event);
    }

public:
    explicit UserListItem(QIcon icon, std::string label, bool switchable, QWidget *parent = nullptr)
        : QPushButton(parent), m_switchable(switchable) {
        setFixedHeight(60);
        setFixedWidth(Sidebar::WIDTH);

        // Setup Layout
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 0, 8, 0);
        layout->setSpacing(12);

        // Icon Widget
        m_iconWidget = new PixmapWidget(icon.pixmap(40, 40), this);
        m_iconWidget->setFixedSize(40, 40);
        layout->addWidget(m_iconWidget);

        // Label
        m_labelWidget = new QLabel(QString::fromStdString(label), this);
        m_labelWidget->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        layout->addWidget(m_labelWidget, 1);

        // Delete Button placeholder
        m_deleteButton = new IconButton(LucideIcons::trash, this);
        m_deleteButton->setFixedSize(24, 24);
        m_deleteButton->setFlat(true);
        layout->addWidget(m_deleteButton);
        m_deleteButton->hide();
        m_deleteButton->setToolTip("Remove profile");

        setLayout(layout);

        // Setup fade animation for hover effect
        m_fadeAnimation = new QPropertyAnimation(this, "hoverAlpha", this);
        m_fadeAnimation->setDuration(225);
        m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

        updateStyleSheet();
    }

    bool switchable() const { return m_switchable; }
    bool selected() const { return m_selected; }

    void setSelected(bool selected) {
        if (m_selected != selected) {
            m_selected = selected;
            updateStyleSheet();
            emit selectedChanged();
        }
    }

    std::string label() const { return m_labelWidget->text().toStdString(); }
};

class SwitchUserDialogContent : public DialogContent {
public:

  void startNewWithProfile(const QString& profileId) {
          QStringList args = QCoreApplication::arguments();

          // 1. Isolate the executable path (index 0)
          QString program = args.takeFirst();

          // 2. Remove existing "--profile <id>" pairs if present
          for (int i = 0; i < args.size(); ++i) {
              if (args.at(i) == "--profile") {
                  args.removeAt(i); // Remove "--profile"
                  if (i < args.size()) {
                      args.removeAt(i); // Remove the <id> following it
                  }
                  break;
              }
          }

          // 3. Append the new flag and ID as separate arguments
          args << "--profile" << profileId; // or QString(profileId) if already a string

          // 4. Launch the detached process
          QProcess::startDetached(program, args);
  }

  SwitchUserDialogContent() {
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);

    /*
    auto* incompleteLabel = new QLabel("<i>incomplete feature do not use pls thx</i>");
    incompleteLabel->setFont(QFont("Quicksand", 9));
    layout->addWidget(incompleteLabel);
    */

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
      QIcon icon(s->value(STK_PFP, ":/defaultuserprofile.png").toString());
      auto name = s->value(STK_DISPLAYNAME, profileId).toString();
      
      auto* button = new UserListItem(icon, name.toStdString(), false, container);
      button->setMaximumWidth(QWIDGETSIZE_MAX);
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      


      connect(button, &UserListItem::clicked, this, [this,profileId]() {
          startNewWithProfile(profileId);
          closeDialog();
      });
        
      userListLayout->addWidget(button);
    }

    userList->setWidget(container);
    layout->addWidget(userList);

    layout->addSpacing(1);

    auto* addProfileButton = new IconButton(LucideIcons::plus);
    layout->addWidget(addProfileButton, 0, Qt::AlignRight);
    connect(addProfileButton, &IconButton::clicked, this, &SwitchUserDialogContent::onNewProfileButton);

  }


  void onNewProfileButton() {
    auto id = generate_uuid_v4();
    startNewWithProfile(QString::fromStdString(id));
    closeDialog();
  }

};

void showSwitchUserDialog() {
  Dialog::showContentDialog(getMainWindow(), "Switch user", new SwitchUserDialogContent(), {300, 500});
}

#include "SwitchUserDialog.moc"
