#include "Sidebar.hpp"
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <algorithm>
#include <QScrollArea>
#include <QFrame>
#include <qboxlayout.h>

void GradientBackground2::paintEvent(QPaintEvent* event) {
    GradientBackground::paintEvent(event);
    
    QPainter painter(this);
    // the "polka dots", as it was called apparently
    painter.setBrush(QColor("#e6d7cf"));
    painter.setPen(Qt::NoPen);

    int dotSize = 7; // Size of the dots
    int line = 1;
    qreal m = static_cast<qreal>(height())/5*2;
    for (int y = 0; y < m; y += dotSize * 2) {
        painter.setOpacity(1-std::max(static_cast<qreal>(y)/(m),0.0)); // just in case there wasnt a implicit bounds in setOpacity or whatever
        for (int x = dotSize/2 + ((line % 2 == 0) ? dotSize : 0); x < width(); x += dotSize * 2) {
            painter.drawEllipse(x, y, dotSize, dotSize);
        }
        line++;
    }
}

void SidebarItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        setSelected(true);
        emit clicked(); // Emit your signal when left-clicked
    }
    
    // Pass the event to the base class if needed
    QWidget::mousePressEvent(event);
}
SidebarItem::SidebarItem(QIcon icon, std::string label, QWidget *parent) : QWidget(parent) {
  setFixedHeight(50); // Set a fixed height for each sidebar item
  setFixedWidth(Sidebar::WIDTH);
  // Setup animation (duration: 200 ms)
  m_fadeAnimation = new QPropertyAnimation(this, "hoverAlpha", this);
  m_fadeAnimation->setDuration(200);
  m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

  m_icon = icon;
  m_label = std::move(label);
}
void SidebarItem::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event);
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_hoverAnimationValue);
    m_fadeAnimation->setEndValue(255); // Max hover opacity (0-255 scale; 100 is ~40%)
    m_fadeAnimation->start();
}

void SidebarItem::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_hoverAnimationValue);
    m_fadeAnimation->setEndValue(0); // Fade back to fully transparent
    m_fadeAnimation->start();
}

void SidebarItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Only paint the background when visible
    if (m_selected || m_hoverAnimationValue > 0) {
        // Horizontal gradient from left to right across the widget's rect
        QLinearGradient gradient(rect().topLeft(), rect().topRight());

        // Target color (e.g., White fading out) modulated by m_hoverAlpha

        QColor startColor = m_selected ? sm_iconColor : sm_hoverColorL; // also icon color 
        QColor stopColor = m_selected ? sm_textColor : sm_hoverColorR; // also text color
        if (!m_selected) {
          startColor.setAlpha(m_hoverAnimationValue);
          stopColor.setAlpha(m_hoverAnimationValue);
        }

        gradient.setColorAt(0.0, startColor);
        gradient.setColorAt(1.0, stopColor);

        painter.fillRect(rect(), gradient);
    }

    // icons and the text. color is the interpolation between its default color and white with the value of the hover animation
    QColor iconColor = sm_iconColor;
    QColor textColor = sm_textColor;
    if (!m_selected) {
      iconColor.setRedF(iconColor.redF() + (1.0 - iconColor.redF()) * (m_hoverAnimationValue / 255.0));
      iconColor.setGreenF(iconColor.greenF() + (1.0 - iconColor.greenF()) * (m_hoverAnimationValue / 255.0));
      iconColor.setBlueF(iconColor.blueF() + (1.0 - iconColor.blueF()) * (m_hoverAnimationValue / 255.0));
      textColor.setRedF(textColor.redF() + (1.0 - textColor.redF()) * (m_hoverAnimationValue / 255.0));
      textColor.setGreenF(textColor.greenF() + (1.0 - textColor.greenF()) * (m_hoverAnimationValue / 255.0));
      textColor.setBlueF(textColor.blueF() + (1.0 - textColor.blueF()) * (m_hoverAnimationValue / 255.0));
    } else {
      iconColor = QColorConstants::White;
      textColor = QColorConstants::White;
    }
    // Draw the icon
    if (!m_icon.isNull()) {
        QPixmap pixmap = m_icon.pixmap(24, 24); // Adjust size
        QPixmap coloredPixmap(pixmap.size());
        coloredPixmap.fill(Qt::transparent);
        QPainter iconPainter(&coloredPixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_Source);
        iconPainter.drawPixmap(0, 0, pixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(coloredPixmap.rect(), iconColor);
        iconPainter.end();
        painter.drawPixmap(10, (height() - 24) / 2, coloredPixmap); // Adjust position
    }

    // Draw the label
    painter.setPen(textColor);
    painter.setFont(QFont("Quicksand", 12, QFont::Weight::Bold));
    painter.drawText(50, 0, width() - 50, height(), Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(m_label));
}

QDebug operator<<(QDebug debug, const SidebarItem *widget) {
  debug.nospace() << "SidebarItem(" << widget->label().c_str() << ")";
  return debug.space();
}

Sidebar::Sidebar(QWidget *parent) : GradientBackground2(parent) {
  // Set a fixed width for the sidebar
  setFixedWidth(Sidebar::WIDTH);

  auto sidebarLayout = new QVBoxLayout(this);
  sidebarLayout->setAlignment(Qt::AlignmentFlag::AlignTop);
  sidebarLayout->setContentsMargins(0, 0, 0, 0); // Spacing inside the layout itself
  sidebarLayout->setSpacing(0);

  m_topSection = new QWidget(this);
  m_topSectionLayout = new QVBoxLayout(m_topSection);
  m_topSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  sidebarLayout->addWidget(m_topSection);
  m_topSectionLayout->setAlignment(Qt::AlignmentFlag::AlignTop);
  m_topSectionLayout->setContentsMargins(0, 0, 0, 0); // Spacing inside the layout itself
  m_topSectionLayout->setSpacing(0);
  m_topSectionLayout->setSizeConstraint(QLayout::SetFixedSize);

  m_scrollSection = new QScrollArea(this);
  m_scrollSectionLayout = new QVBoxLayout(m_scrollSection);
  m_scrollSection->setWidgetResizable(true);
  m_scrollSection->setFrameStyle(QFrame::NoFrame);
  m_scrollSectionLayout->setAlignment(Qt::AlignmentFlag::AlignTop);
  m_scrollSectionLayout->setContentsMargins(0, 0, 0, 0); // Spacing inside the layout itself
  m_scrollSectionLayout->setSpacing(0);
 
  sidebarLayout->addWidget(m_scrollSection);


  m_bottomSection = new QWidget(this);
  m_bottomSectionLayout = new QVBoxLayout(m_bottomSection);
  m_bottomSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  sidebarLayout->addWidget(m_bottomSection);
  m_bottomSectionLayout->setAlignment(Qt::AlignmentFlag::AlignBottom); // TODO:  
  m_bottomSectionLayout->setContentsMargins(0, 0, 0, 0); // Spacing inside the layout itself
  m_bottomSectionLayout->setSpacing(0);
  m_bottomSectionLayout->setSizeConstraint(QLayout::SetFixedSize);

}

Sidebar::~Sidebar() {
  m_topSectionLayout->deleteLater();
  m_scrollSectionLayout->deleteLater();
  m_bottomSectionLayout->deleteLater();

  m_topSection->deleteLater();
  m_scrollSection->deleteLater();
  m_bottomSection->deleteLater();

  layout()->deleteLater();
}

SidebarItem* Sidebar::addSidebarItem(QIcon icon, std::string name, SidebarPosition position, bool selected) {
  QLayout* targetLayout = nullptr;

  switch (position) {
    case SidebarPosition::Top:
        targetLayout = m_topSectionLayout;
        break;
    case SidebarPosition::Scroll:
        targetLayout = m_scrollSectionLayout;
        break;
    case SidebarPosition::Bottom:
        targetLayout = m_bottomSectionLayout;
        break;
    default:
        return nullptr; // Invalid position
  }

  // the widget that uses this layout
  auto layoutedWidget = targetLayout->parentWidget();

  if (targetLayout) {
    auto* item = new SidebarItem(icon, name, layoutedWidget);
    targetLayout->addWidget(item);
    connect(item, &SidebarItem::selectedChanged, this, std::bind(&Sidebar::onSidebarItemClicked, this, item));
    item->setSelected(selected);

    return item;
  }
  return nullptr;
}
QWidget* findChildWidgetBy(QLayout* layout, std::function<bool(QWidget*)> predicate) {
    if (!layout) return nullptr;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        // Check if the item is a QWidget
        if (QWidget* widget = item->widget()) {
            if (predicate(widget)) {
                return widget;
            }
        }
        
        /*
        // Optional: Recurse into sub-layouts
        if (QLayout* childLayout = item->layout()) {
            if (QWidget* found = findChildWidgetBy(childLayout, predicate)) {
                return found;
            }
        }
        */
    }

    return nullptr;
}


void Sidebar::onSidebarItemClicked(SidebarItem* item) {
  if (!item->selected()) return;
  auto _selectedSidebarPredicate = [item](QWidget* widget) {
    auto* i = dynamic_cast<SidebarItem*>(widget);
    if (!i) return false;

    // comparing pointers????
    return i != item && i->selected();
  };
  SidebarItem* maybeSelectedItem = static_cast<SidebarItem*>(findChildWidgetBy(m_topSectionLayout, _selectedSidebarPredicate));

  if (!maybeSelectedItem) {
    maybeSelectedItem = static_cast<SidebarItem*>(findChildWidgetBy(m_scrollSectionLayout, _selectedSidebarPredicate));
  }
  if (!maybeSelectedItem) {
    maybeSelectedItem = static_cast<SidebarItem*>(findChildWidgetBy(m_bottomSectionLayout, _selectedSidebarPredicate));
  }
  
  if (!maybeSelectedItem) {
    qDebug() << "Cannot find another selected sidebar item.";
  } else {
    maybeSelectedItem->setSelected(false);
  }
  selectedItemChanged(maybeSelectedItem, item);
}

int Sidebar::itemPositionOf(SidebarItem* item) {
  // find the layout this item belongs to, and return its index
  // (if its in any other layouts other than the top one, add the number of children from the previous layouts to the value)
  int index = -1;
  int offset = 0;

#define eugh(layout) \
  if (index != -1) {\
    do { \
      auto widget_children = layout->findChildren<QWidget*>(Qt::FindDirectChildrenOnly); \
      if (widget_children.contains(item)) { \
        index = widget_children.indexOf(item); \
        break; \
      } else { \
        offset += widget_children.size(); \
      } \
    } while (0);\
  }
  eugh(m_topSectionLayout);
  eugh(m_scrollSectionLayout);
  eugh(m_bottomSectionLayout);

  return offset+index;
};

