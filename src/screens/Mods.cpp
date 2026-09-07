#include "Mods.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/IconButton.hpp"
#include "../ui/Dialog.hpp"
#include "../utils/LucideIcons.hpp"
#include "../utils/anime.hpp"
#include "../MainWindow.hpp"

#include "../utils/utils.hpp"
#include "ModInfo.hpp"

#include <QWidget>
#include <QPixmap>
#include <QString>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cpptrace/from_current_macros.hpp>
#include <QFileDialog>
#ifndef _NDEBUG
#include <cpptrace/from_current.hpp>
#endif

class DesktopIconWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopIconWidget(const QPixmap &icon, const QString &label, QWidget *parent = nullptr)
    : QWidget(parent), m_icon(icon), m_label(label)
    {
        // Enable mouse tracking so hover detection works properly
        setMouseTracking(true);
        
        // Windows icons are typically fixed in size inside a grid
        setFixedSize(80, 90);
    }

    bool isSelected() const { return m_isSelected; }
    void setSelected(bool selected) {
        if (m_isSelected != selected) {
            m_isSelected = selected;
            update(); // Trigger repaint
        }
    }

signals:
    void clicked();
    void doubleClicked();
    void deselectOtherIconsEvent(DesktopIconWidget* icon);

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        // 1. Draw Hover / Selection Background Box (Windows 10/11 style)
        if (m_isSelected || m_isHovered) {
            QColor bgColor;
            QColor borderColor;

            if (m_isSelected) {
                // Semi-opaque blue selection background and border
                //bgColor = QColor(0, 120, 215, 60);
                //borderColor = QColor(0, 120, 215, 180);
                bgColor = QColor(255, 255, 255, 90);
                borderColor = QColor(255, 255, 255, 130);
            } else {
                // Lighter semi-opaque white/gray hover background
                bgColor = QColor(255, 255, 255, 40);
                borderColor = QColor(255, 255, 255, 80);
            }

            QPainterPath path;
            path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

            painter.fillPath(path, bgColor);
            painter.setPen(QPen(borderColor, 1));
            painter.drawPath(path);
        }

        // 2. Draw Icon (Centered in top portion)
        const int iconSize = 48;
        int iconX = (width() - iconSize) / 2;
        int iconY = 6;
        
        QPixmap scaledIcon = m_icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(iconX, iconY, scaledIcon);

        // 3. Draw Label (Centered below icon, multi-line wrapping)
        QRect labelRect(4, iconY + iconSize + 4, width() - 8, height() - (iconY + iconSize + 6));
        
        // Windows desktop labels typically have white text with a soft shadow over dynamic backgrounds
        painter.setPen(QColor(0, 0, 0, 160)); // Text shadow
        painter.drawText(labelRect.translated(1, 1), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_label);

        painter.setPen(Qt::white); // Main text
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_label);
    }
    void enterEvent(QEnterEvent *event) override {
        m_isHovered = true;
        update();
        QWidget::enterEvent(event);
    }
    void leaveEvent(QEvent *event) override {
        m_isHovered = false;
        update();
        QWidget::leaveEvent(event);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            setSelected(true); // Toggle selection state for demonstration
            // TODO: by default, not using shift deselects other selected icons
            if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
              emit deselectOtherIconsEvent(this);
            }
            emit clicked();
        }
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            emit doubleClicked();
        }
        QWidget::mouseDoubleClickEvent(event);
    }
    QSize sizeHint() const override {return {80,90};}

private:
    QPixmap m_icon;
    QString m_label;
    bool m_isHovered = false;
    bool m_isSelected = false;

    // putting this here for now
    const bool m_doubleClickToOpen = true;
};




ModsScreen::ModsScreen(QWidget *parent) : QWidget(parent) {
  // Set up the layout for the Mods screen
  auto *layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(0,0,0,0);

  m_contentWrapper = new QStackedWidget(this);
  layout->addWidget(m_contentWrapper);

  m_modsListPage = new QWidget();

  auto *modsListPageLayout = new QVBoxLayout(m_modsListPage);
  modsListPageLayout->setContentsMargins(margin, margin, margin, margin);
  modsListPageLayout->setSpacing(4);
  modsListPageLayout->setAlignment(Qt::AlignTop);

  m_contentWrapper->addWidget(m_modsListPage);

  auto* header = new QWidget(m_modsListPage);
  header->setStyleSheet(
    "background-color: white; "
  );
  header->setFixedHeight(40);
  header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  modsListPageLayout->addWidget(header);

  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(8, 0, 8, 0);
  headerLayout->setAlignment(Qt::AlignLeft);

  auto* addModIcon = new IconButton(LucideIcons::plus);
  addModIcon->setToolTip("Add a new mod");
  connect(addModIcon, &IconButton::clicked, this, &ModsScreen::onAddModButtonClicked);
  headerLayout->addWidget(addModIcon);

  
  m_listContent = new QWidget();
  modsListPageLayout->addWidget(m_listContent);
  m_contentLayout = new FlowLayout(m_listContent);
  static const int contentMargin = 0;//8;
  m_contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  m_contentLayout->setSpacing(4);
  m_contentLayout->setAlignment(Qt::AlignLeft);

  for (auto& mod : ModsIndex::getMods()) {
    addModItem(mod);
  }


  m_modInfoPage = new ModInfoScreen();
  m_contentWrapper->addWidget(m_modInfoPage);
  connect(m_modInfoPage, &ModInfoScreen::backButtonClicked, [this](){
    m_contentWrapper->setCurrentWidget(m_modsListPage);
    anime::slideFade(m_modsListPage, anime::SlideDirection::Right);
  });

}

void ModsScreen::addModItem(ModsIndex::Mod& mod) {
  auto iconPath = mod.getIconPath();
  QPixmap iconPixmap(iconPath.c_str());
  auto icon = new DesktopIconWidget(iconPixmap, QString::fromStdString(mod.name), m_listContent);
  m_contentLayout->addWidget(icon);
  connect(icon, &DesktopIconWidget::deselectOtherIconsEvent, this, &ModsScreen::deselectOtherItems);
  connect(icon, &DesktopIconWidget::doubleClicked, this, &ModsScreen::openModInfo);
}

void ModsScreen::openModInfo() {
  m_contentWrapper->setCurrentWidget(m_modInfoPage);
  anime::slideFade(m_modInfoPage, anime::SlideDirection::Left);
}

void ModsScreen::deselectOtherItems(DesktopIconWidget* selectedItem) {
  // abusing the logic of this, sorry
  findChildWidgetBy(m_contentLayout, [selectedItem](QWidget* wid){
    auto icon = dynamic_cast<DesktopIconWidget*>(wid);
    if (!icon) return false; // might be redundant
    if (icon != selectedItem && icon->isSelected()) {
      icon->setSelected(false);
    };
    return false;
  });
}

void ModsScreen::onAddModButtonClicked() {
  auto directory = QFileDialog::getExistingDirectory(nullptr, "Select a mod directory containing a _valid Ren'Py game structure_ to add.");
  if (directory == "") return;
  CPPTRACE_TRY {
    auto m = ModsIndex::installMod(directory.toStdString());
    addModItem(m); 
  } CPPTRACE_CATCH (std::exception& e) {
#ifndef _NDEBUG
    qDebug() << "Exception:" << e.what();
    cpptrace::from_current_exception().to_string();
#endif
    Dialog::showDialog(
      getMainWindow(), 
      "Install Error", 
      "An error was occured while installing the mod.",
      e.what(),
      Dialog::DialogType::Confirm
    );
  }
}

#include "Mods.moc"
