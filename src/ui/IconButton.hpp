#ifndef IconButton_H
#define IconButton_H

#include <QWidget>
#include <QPropertyAnimation>

class IconButton : public QWidget {
  Q_OBJECT 
  Q_PROPERTY(int hoverAlpha READ hoverAlpha WRITE setHoverAlpha)
signals:
  void clicked();
  void selectedChanged();
private:
  int m_hoverAnimationValue = 0; // Ranges from 0 (invisible) to 255 (full opacity)
  QPropertyAnimation *m_fadeAnimation = nullptr;

  QIcon m_icon;

  const QColor sm_borderColor = QColor(137,35,137); 
  const QColor sm_iconColor = QColor(233,25,133);
  const QColor sm_hoverColorL = QColor(197,166,196);
  const QColor sm_hoverColorR = QColor(227,162,195);

  bool m_selected = false;
  bool m_selectable = false;
  int hoverAlpha() const { return m_hoverAnimationValue; }
  void setHoverAlpha(int alpha);
  void mousePressEvent(QMouseEvent *event) override; 
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
};


#endif
