#include "Dialog.hpp"
#include <QFont>
#include <QApplication>
#include <QPainterPath>
#include <qgraphicseffect.h>

#include "../utils/EventFilters.hpp"
#include "utils/LucideIcons.hpp"

// ==========================================
// Button Implementation
// ==========================================
Button::Button(const QString& text, QWidget* parent)
    : QPushButton(text, parent),
      m_primaryColor(QColor(137, 35, 137)),
      m_secondaryColor(QColor(220, 50, 150)), // Adjust target hover color here
      m_currentColor(m_primaryColor)
{
    QFont font("Quicksand", 11, QFont::Bold);
    setFont(font);
    setCursor(Qt::PointingHandCursor);

    // Setup hover animation
    m_colorAnimation.setDuration(250); // 0.25s duration
    connect(&m_colorAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setTextColor(value.value<QColor>());
    });

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setMinimumSize(sizeHint());
}

void Button::setTextColor(const QColor& color) {
    if (m_currentColor != color) {
        m_currentColor = color;
        update(); // Trigger re-paint with updated color
    }
}

void Button::startColorTransition(const QColor& start, const QColor& end) {
    m_colorAnimation.stop();
    m_colorAnimation.setStartValue(start);
    m_colorAnimation.setEndValue(end);
    m_colorAnimation.start();
}

void Button::enterEvent(QEnterEvent* event) {
    QPushButton::enterEvent(event);
    startColorTransition(m_currentColor, m_secondaryColor);
}

void Button::leaveEvent(QEvent* event) {
    QPushButton::leaveEvent(event);
    startColorTransition(m_currentColor, m_primaryColor);
}

void Button::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. Draw Background based on Press/Hover state
    QColor bgColor = QColorConstants::Transparent;//("#ffffff");
    if (isDown()) {
        bgColor = QColor("#ffe0f0");
    } else if (underMouse()) {
        bgColor = QColor("#fff2f8");
    }

    QRectF rectBounds = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(rectBounds, 4, 4);

    painter.fillPath(path, bgColor);

    // 2. Draw Border
    QPen borderPen(m_primaryColor, 1.5);
    painter.setPen(borderPen);
    painter.drawPath(path);

    // 3. Draw Text with Animated Color
    painter.setFont(font());
    painter.setPen(m_currentColor);
    painter.drawText(rect(), Qt::AlignCenter, text());
}

QSize Button::sizeHint() const {
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text());
    
    // Add horizontal padding (16px left + 16px right = 32px)
    int width = qMax(textWidth + 32, 120);
    return QSize(width, 32);
}

// ==========================================
// OverlayWidget Implementation (Backdrop Tint)
// ==========================================
OverlayWidget::OverlayWidget(QWidget* parent) : QWidget(parent) {
    if (parent) {
        setGeometry(parent->rect());

        ChildResizerFilter* filter = new ChildResizerFilter(this, parent);
        parent->installEventFilter(filter);
    }
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

}

void OverlayWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    // Dark semi-transparent overlay to tint background
    painter.fillRect(rect(), QColor(0, 0, 0, 110)); 
}

void OverlayWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

class ClickEventFilter : public QObject
{
  Q_OBJECT

public:
  explicit ClickEventFilter(QObject *parent = nullptr) : QObject(parent) {}

signals:
  // Signal emitted when a valid click is detected
  void clicked(QWidget *target);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    QWidget *widget = qobject_cast<QWidget*>(watched);
    if (!widget) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_mousePressed = true;
            // Return true here if you want to consume/block the press event
            return false; 
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_mousePressed) {
            m_mousePressed = false;

            // Ensure the release occurred within the target widget's area
            if (widget->rect().contains(mouseEvent->position().toPoint())) {
                emit clicked(widget);
                // Return true if you want to consume the click event
                return false; 
            }
        }
        break;
    }
    default:
        break;
    }

    // Pass the event on to the base class / target object
    return QObject::eventFilter(watched, event);
  }

private:
    bool m_mousePressed = false;
};

// ==========================================
// Dialog Implementation
// ==========================================
Dialog::Dialog(const QString& title, 
                QWidget* content,
                const QString& openSfx,
                bool enterEffect,
                QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Widget) , m_openSfx(this)
{
    if (parent) {
        m_overlay = new OverlayWidget(parent);
        m_overlay->show();
        connect(m_overlay, &OverlayWidget::clicked, this, &QDialog::reject);
    }

    setWindowTitle(title);

    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(450, 260);

    // Root layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, BOTTOM_BAR_HEIGHT);
    mainLayout->setSpacing(0);

    // 1. Title Bar
    QWidget* titleBar = new QWidget(this);
    titleBar->setFixedHeight(TITLE_BAR_HEIGHT);
    titleBar->setStyleSheet("background: white;");
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 6, 15, 0);

    QLabel* titleLabel = new QLabel(title, titleBar);
    titleLabel->setFont(QFont("Quicksand", 11, QFont::ExtraBold));
    titleLabel->setStyleSheet("color: rgb(137, 35, 137); background: transparent;");
    titleLayout->addWidget(titleLabel);

    titleLayout->addSpacing(1);

    QLabel* closeButton = new QLabel(titleBar);
    closeButton->setPixmap(LucideIcons::x.pixmap({16, 16}));
    closeButton->setCursor(Qt::PointingHandCursor);
    auto* eventFilter = new ClickEventFilter(closeButton);
    connect(eventFilter, &ClickEventFilter::clicked, this, [this](QWidget*) {
        this->reject();
    });
    closeButton->installEventFilter(eventFilter);
    titleLayout->addWidget(closeButton, 0, Qt::AlignRight);

    mainLayout->addWidget(titleBar);

    mainLayout->addWidget(content);

    m_openSfx.setSource(QUrl(openSfx));

    if (enterEffect) {
      m_enterEffectProgress = 0.0;
      auto *effect = new QGraphicsOpacityEffect(content);
      content->setGraphicsEffect(effect);

      auto *anim = new QVariantAnimation(this);
      anim->setDuration(400);
      anim->setKeyValueAt(0, 0.0);
      anim->setKeyValueAt(0.6, 0.0);
      anim->setKeyValueAt(1.0, 1.0);
      //anim->setEasingCurve(QEasingCurve::OutCubic);

      // Receive calculated values directly in a C++ lambda
      connect(anim, &QVariantAnimation::valueChanged, this, [this, effect](const QVariant &value) {
          m_enterEffectProgress = value.toFloat();
          effect->setOpacity(m_enterEffectProgress);
          
          update(); 
      });

      // Auto-delete when finished
      //connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);

      anim->start(QVariantAnimation::DeleteWhenStopped);
    }

    connect(this, &QDialog::finished, this, [this](int result) {
        if (m_overlay) {
            m_overlay->deleteLater();
            m_overlay = nullptr;
        }
        m_enterEffectProgress = 0;
        update();
    });
}

inline QColor editColor(QColor base, float a) {
  base.setAlphaF(a);

  return base;
}

void Dialog::paintEvent(QPaintEvent* /*event*/) {
    static const int titleBarHeight = TITLE_BAR_HEIGHT;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // initial fill with white
    if (m_enterEffectProgress < 1.0) {
        painter.fillRect(rect(), QColor("#ffffff"));
    }

    float alpha = m_enterEffectProgress;

    // Main background vertical gradient
    QLinearGradient bgGradient(0, titleBarHeight, 0, height()-titleBarHeight-BOTTOM_BAR_HEIGHT);
    bgGradient.setColorAt(0, editColor({"#fff7e7"},alpha));
    bgGradient.setColorAt(1, editColor({"#ffffff"},alpha));
    painter.fillRect(QRect{0,titleBarHeight,width(),height()-BOTTOM_BAR_HEIGHT-titleBarHeight}, bgGradient);

    // Pink bottom bar horizontal gradient
    QLinearGradient bottomGradient(0, 0, width(), 0);
    bottomGradient.setColorAt(0, editColor({"#f28bc1"},alpha));
    bottomGradient.setColorAt(1, editColor({"#f38ac3"},alpha));

    painter.setBrush(bottomGradient);
    painter.setPen(Qt::NoPen);

    int rectHeight = BOTTOM_BAR_HEIGHT;
    int rectY = height() - rectHeight;
    painter.drawRect(0, rectY, width(), rectHeight);
}

void Dialog::showEvent(QShowEvent* event) {
  m_openSfx.play();
  QDialog::showEvent(event);
}

// Static function handling tinted background logic automatically
bool Dialog::showActionDialog(QWidget* parent, 
                              const QString& title, 
                              const QString& message, 
                              const QString& detailText, 
                              DialogType type,
                              bool danger) 
{
    auto* content = new QWidget();

    // 2. Central Content Area
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(25, 20, 25, 15);
    contentLayout->setAlignment(Qt::AlignCenter);

    QLabel* msgLabel = new QLabel(message);
    msgLabel->setFont(QFont("Quicksand", 12, QFont::Bold));
    msgLabel->setStyleSheet("color: rgb(137, 35, 137); background: transparent;");
    msgLabel->setAlignment(Qt::AlignCenter);
    msgLabel->setWordWrap(true);
    contentLayout->addWidget(msgLabel);

    if (!detailText.isEmpty()) {
        contentLayout->addSpacing(8);
        QLabel* detailLabel = new QLabel(detailText);
        detailLabel->setFont(QFont("Quicksand", 11, QFont::Bold));
        detailLabel->setStyleSheet("color: rgb(137, 35, 137); background: transparent;");
        detailLabel->setAlignment(Qt::AlignCenter);
        detailLabel->setWordWrap(true);
        contentLayout->addWidget(detailLabel);
    }

    contentLayout->addStretch();

    // 3. Action Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(25);
    buttonLayout->setAlignment(Qt::AlignCenter);


    contentLayout->addLayout(buttonLayout);

    Dialog dlg(title, content, danger ? "qrc:/audio/dialog_danger_open.wav" : "qrc:/audio/dialog_open.wav", false, parent);

    if (type == YesNo) {
        Button* btnYes = new Button("Yes", &dlg);
        Button* btnNo  = new Button("No", &dlg);

        connect(btnYes, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(btnNo,  &QPushButton::clicked, &dlg, &QDialog::reject);

        buttonLayout->addWidget(btnYes);
        buttonLayout->addWidget(btnNo);
    } else { // Confirm (1 button)
        Button* btnOk = new Button("OK", &dlg);
        connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
        buttonLayout->addWidget(btnOk);
    }
    if (parent) {
        // Center dialog over parent window
        dlg.move(parent->geometry().center() - dlg.rect().center());
    }

    int result = dlg.exec();

    return (result == QDialog::Accepted);
}

void Dialog::showContentDialog(QWidget* parent, 
                       const QString& title, 
                       QWidget* content,
                       QSize size) {
  Dialog dlg(title, content, "qrc:/audio/sidebar_click.wav", true, parent);
  if (size.isValid()) {
    dlg.setFixedSize(size);
  }

  if (parent) {
      // Center dialog over parent window
      dlg.move(parent->geometry().center() - dlg.rect().center());
  }

  dlg.exec();
}

#include "Dialog.moc"
