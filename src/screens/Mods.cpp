#include "Mods.hpp"
#include <QLayout>
#include <QLabel>
#include "../ui/IconButton.hpp"
#include "../ui/MESWidgets.hpp"
#include "../utils/LucideIcons.hpp"
#include "../MainWindow.hpp"

#include "../utils/utils.hpp"
#include "../utils/macros.h"
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
#include <QLineEdit>
#include <qtextlayout.h>
#ifdef MVC_DEBUG
#include <cpptrace/from_current.hpp>
#include <cpptrace/from_current_macros.hpp>
#endif

namespace detail {

DesktopIconWidget::DesktopIconWidget(const QPixmap &icon, const QString &label,
                                     QWidget *parent)
    : QWidget(parent), m_icon(icon), m_label(label) {
  // Enable mouse tracking so hover detection works properly
  setMouseTracking(true);

  // Windows icons are typically fixed in size inside a grid
  setFixedSize(80, 90);

  setToolTip(label);
}
bool DesktopIconWidget::isSelected() const { return m_isSelected; }
void DesktopIconWidget::setSelected(bool selected) {
  if (m_isSelected != selected) {
    m_isSelected = selected;
    update(); // Trigger repaint
    emit this->selected(m_isSelected);
  }
}
QString DesktopIconWidget::label() const { return m_label; }
void DesktopIconWidget::paintEvent(QPaintEvent *event) {
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
      // bgColor = QColor(0, 120, 215, 60);
      // borderColor = QColor(0, 120, 215, 180);
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

  QPixmap scaledIcon = m_icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
  painter.drawPixmap(iconX, iconY, scaledIcon);

  // 3. Draw Label (Centered below icon, multi-line wrapping)
  QRect labelRect(4, iconY + iconSize + 12, width() - 8,
                  height() - (iconY + iconSize + 6));
  QTextLayout textLayout(m_label, painter.font());
  QTextOption textOption;
  textOption.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  textOption.setWrapMode(QTextOption::WordWrap);
  textLayout.setTextOption(textOption);

  textLayout.beginLayout();
  qreal currentY = 0;

  struct LineData {
    QTextLine line;
    qreal yPos;
    qreal xPos;
  };
  QVector<LineData> visibleLines;

  while (true) {
    QTextLine line = textLayout.createLine();
    if (!line.isValid())
      break;

    line.setLineWidth(labelRect.width());

    // Check if this line fits within the allowed label height
    if (currentY + line.height() > labelRect.height() + 7) {
      break; 
    }

    qreal x = (labelRect.width() - line.naturalTextWidth()) / 2;

    visibleLines.append({line, currentY, x});
    currentY += line.height();
  }
  textLayout.endLayout();

  // Draw each visible line
  for (int i = 0; i < visibleLines.size(); ++i) {
    const auto &data = visibleLines[i];
    QTextLine line = data.line;
    QString lineText = m_label.mid(line.textStart(), line.textLength());
    qreal lineX = data.xPos;

    // If this is the last visible line and there is still text remaining, elide it
    if (i == visibleLines.size() - 1 && (line.textStart() + line.textLength() < m_label.length())) {
      QFontMetrics metrics(painter.font());
      lineText = metrics.elidedText(lineText, Qt::ElideRight, labelRect.width());
      if (!lineText.endsWith("...")) {
        lineText = lineText.trimmed() + "...";
      }
      lineX = (labelRect.width() - metrics.horizontalAdvance(lineText)) / 2.0; // Recalculate xPos for elided text
    }

    QPointF linePos(labelRect.left() + lineX, labelRect.top() + data.yPos);

    // Draw text shadow
    painter.setPen(QColor(0, 0, 0, 160));
    painter.drawText(linePos + QPointF(1, 1), lineText);

    // Draw main text
    painter.setPen(Qt::white);
    painter.drawText(linePos, lineText);
  }
}
void DesktopIconWidget::enterEvent(QEnterEvent *event) {
  m_isHovered = true;
  update();
  QWidget::enterEvent(event);
}
void DesktopIconWidget::leaveEvent(QEvent *event) {
  m_isHovered = false;
  update();
  QWidget::leaveEvent(event);
}
void DesktopIconWidget::mousePressEvent(QMouseEvent *event) {
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
void DesktopIconWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    emit doubleClicked();
    return event->accept();
  }
  QWidget::mouseDoubleClickEvent(event);
}
QSize DesktopIconWidget::sizeHint() const { return {80, 90}; }
} // namespace detail

ModsScreen::ModsScreen(Router* router, QWidget *parent) 
  : QWidget(parent), m_router(router) {
  setWindowTitle("Mods");
  // Set up the layout for the Mods screen
  auto *layout = new QVBoxLayout(this);
  static const int margin = 24;

  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);
  layout->setAlignment(Qt::AlignTop);


  auto* hcs = new QStackedWidget(this);
  m_headerContentStack = hcs;
  m_headerContentStack->setObjectName("header");
  m_headerContentStack->setStyleSheet(
    "#header {background-color: white;}"
  );
  m_headerContentStack->setFixedHeight(40);
  m_headerContentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(m_headerContentStack);

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

  // headerLayout->addStretch();
  auto* searchBar = new QLineEdit();
  searchBar->setPlaceholderText("Search mod (by name)");
  // on text edited (not submitted), filter mods grid by hidiing unmatched DesktopIconWidgets
  connect(searchBar, &QLineEdit::textEdited, this, [this](const QString& text){
    findChildWidgetBy(m_contentLayout, [this, text](QWidget* w){
      if (auto icon = qobject_cast<detail::DesktopIconWidget*>(w)) {
        bool match = icon->label().contains(text, Qt::CaseInsensitive);
        icon->setVisible(match);
      }
      return false;
    });
  });
  headerLayout->addWidget(searchBar);

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
      if (auto icon = qobject_cast<detail::DesktopIconWidget*>(w)) {
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
  layout->addWidget(m_listContentStack);


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
      if (auto icon = qobject_cast<detail::DesktopIconWidget*>(w)) {
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
        if (auto icon = qobject_cast<detail::DesktopIconWidget*>(w)) {
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
  auto icon = new detail::DesktopIconWidget(iconPixmap, QString::fromStdString(mod.name), m_listContent);
  icon->setProperty("modId", QString::fromStdString(mod.id));
  m_contentLayout->addWidget(icon);
  connect(icon, &detail::DesktopIconWidget::deselectOtherIconsEvent, this, &ModsScreen::deselectOtherItems);
  connect(icon, &detail::DesktopIconWidget::doubleClicked, this, [mod,this](){openModInfo(mod);});
  connect(icon, &detail::DesktopIconWidget::multiselectStart, this, [this](){enableMultiselectToolbar(true);});
  connect(icon, &detail::DesktopIconWidget::multiselectEnd, this, [this](){enableMultiselectToolbar(false);});
  connect(icon, &detail::DesktopIconWidget::selected, this, &ModsScreen::onItemSelected);

  updateModsCountLabel();
  updateModsListDisplay();
}

void ModsScreen::removeModItem(std::string modId) {
  auto icon = findChildWidgetBy(m_contentLayout, [&modId](QWidget* w){
    if (auto icon = qobject_cast<detail::DesktopIconWidget*>(w)) {
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
  m_router->navigate(QString("/mods/%1").arg(QString::fromStdString(mod.id)));
}

void ModsScreen::deselectOtherItems(detail::DesktopIconWidget* selectedItem) {
  // abusing the logic of this, sorry
  findChildWidgetBy(m_contentLayout, [selectedItem](QWidget* wid){
    auto icon = dynamic_cast<detail::DesktopIconWidget*>(wid);
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
  auto directory = QFileDialog::getExistingDirectory(nullptr, tr("Select a mod directory."));
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

