#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "GradientBackground.hpp"
#include <QScrollArea>
#include <QLayout>
#include <QPropertyAnimation>


class GradientBackground2 : public GradientBackground {
    Q_OBJECT
public:
    explicit GradientBackground2(QWidget *parent = nullptr) : GradientBackground(parent) {}
protected:
    void paintEvent(QPaintEvent* event) override; 
};


class SidebarItem : public QWidget {
  Q_OBJECT 
  Q_PROPERTY(int hoverAlpha READ hoverAlpha WRITE setHoverAlpha)
signals:
  void clicked();
  void selectedChanged();
private:
  int m_hoverAnimationValue = 0; // Ranges from 0 (invisible) to 255 (full opacity)
  QPropertyAnimation *m_fadeAnimation = nullptr;

  QIcon m_icon;
  std::string m_label;

  const QColor sm_textColor = QColor(137,35,137); 
  const QColor sm_iconColor = QColor(233,25,133);
  const QColor sm_hoverColorL = QColor(197,166,196);
  const QColor sm_hoverColorR = QColor(227,162,195);

  bool m_selected = false;
  int hoverAlpha() const { return m_hoverAnimationValue; }
  void setHoverAlpha(int alpha) {
    if (m_hoverAnimationValue != alpha) {
        m_hoverAnimationValue = alpha;
        update(); // Triggers paintEvent() on every animation frame
    }
  }
  void mousePressEvent(QMouseEvent *event) override; 
public:
  // should this be a qbutton then?
  // nah i dont think so
  bool selected() const {return m_selected;}
  void setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        update(); 
        emit selectedChanged();
    }
  }
  std::string label() const {return m_label;}
  explicit SidebarItem(QIcon icon, std::string label, QWidget *parent = nullptr);
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override; 
};

enum class SidebarPosition {
  Top, Scroll, Bottom
};
class Sidebar : public GradientBackground2 {
    Q_OBJECT

    QWidget* m_topSection = nullptr;
    QScrollArea* m_scrollSection = nullptr;
    QWidget* m_bottomSection = nullptr;


    QVBoxLayout* m_topSectionLayout = nullptr;
    QVBoxLayout* m_scrollSectionLayout = nullptr;
    QVBoxLayout* m_bottomSectionLayout = nullptr;

signals:
    void selectedItemChanged(SidebarItem* oldItem, SidebarItem* newItem);
public:
    static constexpr int WIDTH = 200;
    explicit Sidebar(QWidget *parent = nullptr);
    ~Sidebar();

    SidebarItem* addSidebarItem(QIcon icon, std::string name, SidebarPosition position, bool selected = false);
    int itemPositionOf(SidebarItem* item);
private:
    void onSidebarItemClicked(SidebarItem* item);  
}; 

QDebug operator<<(QDebug debug, const SidebarItem *widget);

#endif
