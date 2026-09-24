#ifndef IconButton_H
#define IconButton_H

#include <QPropertyAnimation>
#include <QPushButton>
#include "../consts.hpp"

class IconButton : public QPushButton {
  Q_OBJECT 
  Q_PROPERTY(int hoverAlpha READ hoverAlpha WRITE setHoverAlpha)
signals:
  void selectedChanged();
private:
  int m_hoverAnimationValue = 0; // Ranges from 0 (invisible) to 255 (full opacity)
  QPropertyAnimation *m_fadeAnimation = nullptr;

  QIcon m_icon;

  const QColor sm_borderColor = c_primaryColor; 
  const QColor sm_iconColor = c_secondaryColor;
  const QColor sm_hoverColorL = c_primaryLightColor;
  const QColor sm_hoverColorR = c_secondaryLightColor;
  const QColor sm_iconHoverColor = QColor("#ec50a3");

  bool m_selected = false;
  bool m_selectable = false;
  int hoverAlpha() const { return m_hoverAnimationValue; }
  void setHoverAlpha(int alpha);
  void mouseReleaseEvent(QMouseEvent *event) override; 
public:
  // should this be a qbutton then?
  // nah i dont think so
  bool selected() const {return m_selected;}
  void setSelected(bool selected);
  void setSelectable(bool selectable);
  explicit IconButton(QIcon icon, QWidget *parent = nullptr);
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override; 
protected:
  bool event(QEvent *event) override;
};


#endif
