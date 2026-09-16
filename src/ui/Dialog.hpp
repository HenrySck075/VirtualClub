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
#include <QSoundEffect>
#include <QVariantAnimation>

//  Button Class
class Button : public QPushButton {
    Q_OBJECT
    // Property required for QVariantAnimation target
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)

public:
    explicit Button(const QString& text, QWidget* parent = nullptr);
    Button(const QIcon& icon, QWidget* parent = nullptr);
    Button(const QString& text, const QIcon& icon, QWidget* parent = nullptr);

    QColor textColor() const { return m_currentColor; }
    void setTextColor(const QColor& color);

    void setPrimaryColor(const QColor& color) { m_primaryColor = color; }
    void setSecondaryColor(const QColor& color) { m_secondaryColor = color; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;

private:
    void startColorTransition(const QColor& start, const QColor& end);

    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_currentColor;
    QVariantAnimation m_colorAnimation;
};

// Overlay widget for tinting the background when the dialog is open
class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWidget(QWidget* parent = nullptr);
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
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
                      QWidget* content,
                      const QString& openSfx,
                      bool enterEffect = true,
                      QWidget* parent = nullptr);

    // Call this static helper to launch the dialog over a target parent window
    static bool showActionDialog(QWidget* parent, 
                           const QString& title, 
                           const QString& message, 
                           const QString& detailText = QString(), 
                           DialogType type = YesNo,
                           bool danger = false);
    static void showContentDialog(QWidget* parent, 
                           const QString& title, 
                           QWidget* content,
                           QSize size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    static constexpr int BOTTOM_BAR_HEIGHT = 20;
    static constexpr int TITLE_BAR_HEIGHT = 35;
    
    QColor m_purpleColor{137, 35, 137};

    QSoundEffect m_openSfx;
    float m_enterEffectProgress = 1.0;

    OverlayWidget* m_overlay = nullptr;
};
#endif
