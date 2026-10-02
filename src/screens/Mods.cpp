#include "Mods.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/IconButton.hpp"
#include "../ui/MESWidgets.hpp"
#include "../utils/LucideIcons.hpp"
#include "../utils/anime.hpp"
#include "../MainWindow.hpp"

#include "../utils/utils.hpp"
#include "../utils/macros.h"
#include "ModInfo.hpp"
#include "utils/EventFilters.hpp"
#include "utils/ModIndex.hpp"

#include <QWidget>
#include <QPixmap>
#include <QString>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QSharedPointer>
#ifdef MVC_DEBUG
#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>
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
            emit this->selected(m_isSelected);
        }
    }

signals:
    void clicked();
    void doubleClicked();
    void deselectOtherIconsEvent(DesktopIconWidget* icon);
    void multiselectStart();
    void multiselectEnd();
    void selected(bool s);

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
              emit multiselectEnd();
              emit deselectOtherIconsEvent(this);
            } else {
              emit multiselectStart();
            }
            emit clicked();

            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            emit doubleClicked();
            return event->accept();
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
  setWindowTitle("Mods");
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




  auto* hcs = new QStackedWidget(m_modsListPage);
  m_headerContentStack = hcs;
  m_headerContentStack->setObjectName("header");
  m_headerContentStack->setStyleSheet(
    "#header {background-color: white;}"
  );
  m_headerContentStack->setFixedHeight(40);
  m_headerContentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  modsListPageLayout->addWidget(m_headerContentStack);

  m_headerContent = new QWidget();
  m_headerContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  hcs->addWidget(m_headerContent);

  auto* headerLayout = new QHBoxLayout(m_headerContent);
  headerLayout->setContentsMargins(8, 0, 8, 0);
  headerLayout->setAlignment(Qt::AlignLeft);

  auto* addModIcon = new IconButton(LucideIcons::plus);
  addModIcon->setToolTip(tr("Add a new mod"));
  connect(addModIcon, &IconButton::clicked, this, &ModsScreen::onAddModButtonClicked);
  headerLayout->addWidget(addModIcon);

  headerLayout->addStretch();

  m_modsCountLabel = new QLabel();
  m_modsCountLabel->setFont(QFont("Quicksand", 10));
  updateModsCountLabel();
  headerLayout->addWidget(m_modsCountLabel);

  m_headerContentMultiselect = new QWidget();
  m_headerContentMultiselect->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  hcs->addWidget(m_headerContentMultiselect);

  auto* multiselectLayout = new QHBoxLayout(m_headerContentMultiselect);
  multiselectLayout->setContentsMargins(8, 0, 8, 0);
  multiselectLayout->setAlignment(Qt::AlignLeft);

  auto* deleteButton = new IconButton(LucideIcons::trash);
  connect(deleteButton, &IconButton::clicked, this, [this](){
    if (!Dialog::showActionDialog(
      getMainWindow(), 
      tr("Delete selected mods?"), 
      tr("Are you sure you want to delete the selected mods? This action cannot be undone."),
      "",
      Dialog::YesNo,
      true
    )) return;

    findChildWidgetBy(m_contentLayout, [this](QWidget* w){
      if (auto icon = qobject_cast<DesktopIconWidget*>(w)) {
        if (icon->isSelected()) {
          icon->disconnect();
          m_contentLayout->removeWidget(icon);
          icon->deleteLater();
        }
      }
      return false;
    });

    updateModsCountLabel();
    updateModsListDisplay();
    enableMultiselectToolbar(false);
  });
  multiselectLayout->addWidget(deleteButton);


  m_listContentStack = new QStackedWidget();
  m_listContentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  modsListPageLayout->addWidget(m_listContentStack);


  QLabel* lcp = new QLabel(tr("Install new mods by pressing the + button"));
  lcp->setAlignment(Qt::AlignCenter);
  lcp->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 14px; font-family: Quicksand; font-weight: bold;");
  lcp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  lcp->setWordWrap(true);
  m_listContentPlaceholder = lcp;
  m_listContentStack->addWidget(m_listContentPlaceholder);


  
  m_listContent = new QWidget();
  auto cef = new ClickEventFilter();
  connect(cef, &ClickEventFilter::clicked, this, [this](QWidget*){
    enableMultiselectToolbar(false);
    findChildWidgetBy(m_contentLayout, [this](QWidget* w){
      if (auto icon = qobject_cast<DesktopIconWidget*>(w)) {
        if (icon->isSelected()) {
          icon->setSelected(false);
        }
      }
      return false;
    });
  });
  m_listContent->installEventFilter(cef);
  m_listContentStack->addWidget(m_listContent);
  m_contentLayout = new FlowLayout(m_listContent);
  static const int contentMargin = 0;//8;
  m_contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  m_contentLayout->setSpacing(4);
  m_contentLayout->setAlignment(Qt::AlignLeft);

  for (auto& mod : ModsIndex::get()->getMods()) {
    addModItem(mod);
  }

  updateModsListDisplay();


  m_modInfoPage = new ModInfoScreen();
  m_contentWrapper->addWidget(m_modInfoPage);
  auto backCb = [this](){
    c_navigationSfx->play();
    m_contentWrapper->setCurrentWidget(m_modsListPage);
    setWindowTitle("Mods");
    anime::slideFade(m_modsListPage, anime::SlideDirection::Right);
  };
  connect(m_modInfoPage, &ModInfoScreen::backButtonClicked, backCb);
  connect(m_modInfoPage, &ModInfoScreen::modUninstalled, [this, backCb](std::string modId){
    removeModItem(modId);
    backCb();
  });
  connect(m_modInfoPage, &QWidget::windowTitleChanged, this, [this](const QString& title){
    if (m_contentWrapper->currentWidget() == m_modInfoPage) {
      setWindowTitle(title);
    }
  });

}

void ModsScreen::enableMultiselectToolbar(bool enable) {
  if (enable) {
    m_headerContentStack->setCurrentWidget(m_headerContentMultiselect);
  } else {
    m_headerContentStack->setCurrentWidget(m_headerContent);
  }
}

void ModsScreen::onItemSelected(bool s) {
    if (!s) {
      // check if any other icons are still selected
      bool anySelected = false;
      findChildWidgetBy(m_contentLayout, [&anySelected](QWidget* w){
        if (auto icon = qobject_cast<DesktopIconWidget*>(w)) {
          if (icon->isSelected()) {
            anySelected = true;
            return true; // break the loop
          }
        }
        return false;
      });
      if (!anySelected) {
        enableMultiselectToolbar(false);
      }
    }

}

void ModsScreen::updateModsCountLabel() {
  m_modsCountLabel->setText(QString("Mods: %1").arg(ModsIndex::get()->getMods().size()));
};
void ModsScreen::updateModsListDisplay() {
  if (ModsIndex::get()->getMods().size() != 0) {
    m_listContentStack->setCurrentWidget(m_listContent);
  } else {
    m_listContentStack->setCurrentWidget(m_listContentPlaceholder);
  }
}

void ModsScreen::addModItem(ModsIndex::Mod mod) {
  auto iconPath = mod.getIconPath();
  QPixmap iconPixmap(iconPath.c_str());
  auto icon = new DesktopIconWidget(iconPixmap, QString::fromStdString(mod.name), m_listContent);
  icon->setProperty("modId", QString::fromStdString(mod.id));
  m_contentLayout->addWidget(icon);
  connect(icon, &DesktopIconWidget::deselectOtherIconsEvent, this, &ModsScreen::deselectOtherItems);
  connect(icon, &DesktopIconWidget::doubleClicked, this, [mod,this](){openModInfo(mod);});
  connect(icon, &DesktopIconWidget::multiselectStart, this, [this](){enableMultiselectToolbar(true);});
  connect(icon, &DesktopIconWidget::multiselectEnd, this, [this](){enableMultiselectToolbar(false);});
  connect(icon, &DesktopIconWidget::selected, this, &ModsScreen::onItemSelected);

  updateModsCountLabel();
  updateModsListDisplay();
}

void ModsScreen::removeModItem(std::string modId) {
  auto icon = findChildWidgetBy(m_contentLayout, [&modId](QWidget* w){
    if (auto icon = qobject_cast<DesktopIconWidget*>(w)) {
      return icon->property("modId").toString().toStdString() == modId;
    }
    return false;
  });

  if (!icon) return;
  icon->disconnect();
  m_contentLayout->removeWidget(icon);
  icon->deleteLater();

  updateModsCountLabel();
  updateModsListDisplay();
}

void ModsScreen::openModInfo(const ModsIndex::Mod& mod) {
  c_navigationSfx->play();
  m_contentWrapper->setCurrentWidget(m_modInfoPage);
  m_modInfoPage->setDisplayingMod(mod);
  setWindowTitle(m_modInfoPage->windowTitle());
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

#ifndef MVC_DEBUG
#define CPPTRACE_TRY try
#define CPPTRACE_CATCH(x) catch(x)
#endif

void ModsScreen::onAddModButtonClicked() {
  auto directory = QFileDialog::getExistingDirectory(nullptr, tr("Select a mod directory containing a _valid Ren'Py game structure_ to add."));
  if (directory == "") return;
    
  CPPTRACE_TRY {
    auto m = ModsIndex::get()->installMod(directory.toStdString());
    addModItem(m); 
  } CPPTRACE_CATCH (std::exception& e) {
#ifdef MVC_DEBUG
    qDebug() << "Exception:" << e.what();
    cpptrace::from_current_exception().to_string();
#endif
    Dialog::showActionDialog(
      getMainWindow(), 
      tr("Install Error"), 
      tr("An error was occured while installing the mod."),
      e.what(),
      Dialog::DialogType::Confirm
    );
  }
}

#include "Mods.moc"
