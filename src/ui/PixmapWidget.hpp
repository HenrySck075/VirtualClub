#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPaintEvent>

class PixmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit PixmapWidget(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pixmap);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
};

#endif // IMAGEWIDGET_H
