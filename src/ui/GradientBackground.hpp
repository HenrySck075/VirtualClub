#ifndef GradientBackground_H
#define GradientBackground_H

#include <QWidget>

class GradientBackground : public QWidget {
    Q_OBJECT
public:
    explicit GradientBackground(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground, true);
    }
protected:
    void paintEvent(QPaintEvent *event) override; 
};

#endif
