#include "Dialog.hpp"
#include <QFont>
#include <QApplication>
#include <QPainterPath>
#include <qgraphicseffect.h>

#include "../utils/EventFilters.hpp"
#include "consts.hpp"
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
}

Button::Button(const QIcon& icon, QWidget* parent) : Button("", parent) {
  setIcon(icon);
}

Button::Button(const QString& text, const QIcon& icon, QWidget* parent) : Button(text, parent) {
  setIcon(icon);
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
  if (!isEnabled()) return;
  startColorTransition(m_currentColor, m_secondaryColor);
}

void Button::leaveEvent(QEvent* event) {
  QPushButton::leaveEvent(event);
  if (!isEnabled()) return;
  startColorTransition(m_currentColor, m_primaryColor);
}

void Button::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. Draw Background
    QColor bgColor = QColorConstants::Transparent;
    if (isEnabled()) {
        if (isDown()) {
            bgColor = QColor("#ffe0f0");
        } else if (underMouse()) {
            bgColor = QColor("#fff2f8");
        }
    }

    QRectF rectBounds = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(rectBounds, 4, 4);

    painter.fillPath(path, bgColor);

    // 2. Draw Border
    QPen borderPen(isEnabled() ? m_primaryColor : c_disabledColor, 1.5);
    painter.setPen(borderPen);
    painter.drawPath(path);

    // 3. Define Internal Padding Margins
    int paddingX = 16;
    int paddingY = 4;
    QRect paddedRect = rect().adjusted(paddingX, paddingY, -paddingX, -paddingY);

    // 4. Measure Dimensions
    painter.setFont(font());
    QColor contentColor = isEnabled() ? m_currentColor : c_disabledColor;
    painter.setPen(contentColor);

    QIcon btnIcon = icon();
    QSize icSize = iconSize();
    int spacing = (btnIcon.isNull() || text().isEmpty()) ? 0 : 6;

    QFontMetrics fm(font());
    int textWidth = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    int actualIconWidth = btnIcon.isNull() ? 0 : icSize.width();

    int totalContentWidth = actualIconWidth + spacing + textWidth;

    // Calculate centering starting point inside padded area
    int startX = paddedRect.center().x() - (totalContentWidth / 2);

    // 5. Draw Icon (if present)
    if (!btnIcon.isNull()) {
        int iconY = paddedRect.center().y() - (icSize.height() / 2);
        QRect iconRect(startX, iconY, icSize.width(), icSize.height());

        QIcon::Mode iconMode = isEnabled() ? (isDown() ? QIcon::Active : QIcon::Normal) 
                                           : QIcon::Disabled;
        QIcon::State iconState = isChecked() ? QIcon::On : QIcon::Off;

        QPixmap px = btnIcon.pixmap(icSize, painter.device()->devicePixelRatio(), iconMode, iconState);

        // Tint Icon
        QColor targetIconColor = isEnabled() ? c_secondaryColor : c_disabledColor;
        QPainter pxPainter(&px);
        pxPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        pxPainter.fillRect(px.rect(), targetIconColor);
        pxPainter.end();

        painter.drawPixmap(iconRect, px);
        startX += icSize.width() + spacing;
    }

    // How to verify layout rendering: Check that icon and text remain centered as button width resizes

    // 6. Draw Text
    if (!text().isEmpty()) {
        int textY = paddedRect.center().y() - (fm.height() / 2);
        QRect textRect(startX, textY, textWidth, fm.height());
        
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

QSize Button::sizeHint() const {
    QFontMetrics fm(font());
    int textWidth = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    
    // Measure Icon + Spacing
    int iconWidth = 0;
    if (!icon().isNull()) {
        iconWidth = iconSize().width() + (text().isEmpty() ? 0 : 6); // 6px spacing
    }

    int paddingX = 16; // Left and Right padding (16px * 2 = 32px total)
    int paddingY = 8;  // Top and Bottom padding

    int contentWidth = iconWidth + textWidth;
    int calculatedWidth = contentWidth + (paddingX * 2);
    int calculatedHeight = qMax(iconSize().height(), fm.height()) + (paddingY * 2);

    int finalWidth = qMax(calculatedWidth, 120);
    int finalHeight = qMax(calculatedHeight, 32);

    return QSize(finalWidth, finalHeight);
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
    effect->setOpacity(0);

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
    msgLabel->setAlignment(Qt::AlignCenter);
    msgLabel->setWordWrap(true);
    contentLayout->addWidget(msgLabel);

    if (!detailText.isEmpty()) {
        contentLayout->addSpacing(8);
        QLabel* detailLabel = new QLabel(detailText);
        detailLabel->setFont(QFont("Quicksand", 11, QFont::Bold));
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
