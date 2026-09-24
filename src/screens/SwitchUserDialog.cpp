#include "SwitchUserDialog.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "ui/IconButton.hpp"
#include "ui/Sidebar.hpp" // for reused SidebarItem
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"

#include "ui/MESWidgets.hpp"
#include <qframe.h>

class UserListItem : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(int hoverAlpha READ hoverAlpha WRITE setHoverAlpha)

signals:
    void selectedChanged();
    void clicked();

private:
    int m_hoverAnimationValue = 0; // Ranges from 0 to 255
    QPropertyAnimation *m_fadeAnimation = nullptr;

    QIcon m_icon;
    std::string m_label;

    const QColor sm_textColor = c_primaryColor;
    const QColor sm_iconColor = c_secondaryColor;
    const QColor sm_hoverColorL = c_primaryLightColor;
    const QColor sm_hoverColorR = c_secondaryLightColor;

    bool m_selected = false;
    bool m_hovered = false;
    bool m_switchable = true;
    bool m_tintIcon = true;

    int hoverAlpha() const { return m_hoverAnimationValue; }
    void setHoverAlpha(int alpha) {
        if (m_hoverAnimationValue != alpha) {
            m_hoverAnimationValue = alpha;
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

public:
    explicit UserListItem(QIcon icon, std::string label, bool switchable, QWidget *parent = nullptr);

    bool switchable() const { return m_switchable; }
    bool selected() const { return m_selected; }
    void setSelected(bool selected);

    std::string label() const { return m_label; }
    void setTintIcon(bool enable) { m_tintIcon = enable; }

protected:
    void paintEvent(QPaintEvent *event) override;
};


UserListItem::UserListItem(QIcon icon, std::string label, bool switchable, QWidget *parent)
    : QPushButton(parent), m_icon(icon), m_label(std::move(label)), m_switchable(switchable) {
    setFixedHeight(60);
    setFixedWidth(Sidebar::WIDTH);

    // Setup fade animation for hover effect
    m_fadeAnimation = new QPropertyAnimation(this, "hoverAlpha", this);
    m_fadeAnimation->setDuration(225);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    m_fadeAnimation->setStartValue(0);
    m_fadeAnimation->setEndValue(255);
}

void UserListItem::setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        update();
        emit selectedChanged();
    }
}

void UserListItem::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event);
    m_hovered = true;
    m_fadeAnimation->stop();
    m_fadeAnimation->start();
}

void UserListItem::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    m_hovered = false;
    m_fadeAnimation->stop();
    setHoverAlpha(0);
}

void UserListItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_switchable) {
            setSelected(true);
            emit clicked();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void UserListItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Only paint the background when selected or hovering
    if (m_selected || m_hoverAnimationValue > 0) {
        QLinearGradient gradient(rect().topLeft(), rect().topRight());

        QColor startColor = m_selected ? sm_iconColor : sm_hoverColorL;
        QColor stopColor = m_selected ? sm_textColor : sm_hoverColorR;

        if (!m_selected) {
            startColor.setAlpha(m_hoverAnimationValue);
            stopColor.setAlpha(m_hoverAnimationValue);
        }

        gradient.setColorAt(0.0, startColor);
        gradient.setColorAt(1.0, stopColor);

        painter.fillRect(rect(), gradient);
    }

    // Determine colors based on selection or hover state
    QColor iconColor = sm_iconColor;
    QColor textColor = sm_textColor;
    if (m_selected || m_hovered) {
        iconColor = QColorConstants::White;
        textColor = QColorConstants::White;
    }

    const int iconSize = 40;
    const int iconX = 8;
    const int textX = iconX + iconSize + 12;

    // Draw the enlarged icon (40x40)
    if (!m_icon.isNull()) {
        QPixmap pixmap = m_icon.pixmap(iconSize, iconSize);
        QPixmap coloredPixmap = pixmap;

        if (m_tintIcon) {
            coloredPixmap = QPixmap(pixmap.size());
            coloredPixmap.fill(Qt::transparent);

            QPainter iconPainter(&coloredPixmap);
            iconPainter.setCompositionMode(QPainter::CompositionMode_Source);
            iconPainter.drawPixmap(0, 0, pixmap);
            iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            iconPainter.fillRect(coloredPixmap.rect(), iconColor);
            iconPainter.end();
        }

        painter.drawPixmap(iconX, (height() - iconSize) / 2, coloredPixmap);
    }

    // Draw the label
    painter.setPen(textColor);
    painter.setFont(QFont("Quicksand", 12, QFont::Weight::DemiBold));
    painter.drawText(textX, 0, width() - textX, height(), Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(m_label));
}

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
    QIcon icon(s->value(STK_PFP, ":/defaultuserprofile.png").toString());
    auto name = s->value(STK_DISPLAYNAME, profileId).toString();
    
    auto* button = new UserListItem(icon, name.toStdString(), false, container);
    button->setMaximumWidth(QWIDGETSIZE_MAX);
    button->setTintIcon(false);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    connect(button, &UserListItem::clicked, this, [profileId](){
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

#include "SwitchUserDialog.moc"
