#ifndef MESDialog_H
#define MESDialog_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QLinearGradient>

//  Button Class
class Button : public QPushButton {
    Q_OBJECT
public:
    explicit Button(const QString& text, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;
};

//  Dialog Class
class Dialog : public QDialog {
    Q_OBJECT
public:
    enum DialogType {
        Confirm, // Single OK button
        YesNo    // Yes / No buttons
    };

    explicit Dialog(const QString& title, 
                         const QString& message, 
                         const QString& detailText = QString(),
                         DialogType type = YesNo, 
                         QWidget* parent = nullptr);

    // Call this static helper to launch the dialog over a target parent window
    static bool showDialog(QWidget* parent, 
                           const QString& title, 
                           const QString& message, 
                           const QString& detailText = QString(), 
                           DialogType type = YesNo);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int BOTTOM_BAR_HEIGHT = 20;
    static constexpr int TITLE_BAR_HEIGHT = 30;
    
    QColor m_purpleColor{137, 35, 137};
};

// Overlay widget for tinting the background when the dialog is open
class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};
#endif
