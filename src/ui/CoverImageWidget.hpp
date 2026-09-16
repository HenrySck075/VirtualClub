#ifndef COVERIMAGEWIDGET_H
#define COVERIMAGEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPainter>

class CoverImageWidget : public QWidget {
    Q_OBJECT

public:
    explicit CoverImageWidget(QWidget *parent = nullptr) : QWidget(parent) {}

    void setPixmap(const QPixmap &pixmap) {
        m_pixmap = pixmap;
        update(); // Trigger repaint
    }

    void setImagePath(const QString &path) {
        m_pixmap.load(path);
        update();
    }

    void setCentered(bool on) {m_centered = on;}
    bool centered() {return m_centered;}

protected:
  void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
    bool m_centered = true;
};

#endif
