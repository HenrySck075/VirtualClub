#include "Dialog.hpp"
#include <QFont>
#include <QApplication>

// ==========================================
// Button Implementation
// ==========================================
Button::Button(const QString& text, QWidget* parent)
    : QPushButton(text, parent) 
{
    // Configure default font
    QFont font("Quicksand", 11, QFont::Bold);
    setFont(font);
    setCursor(Qt::PointingHandCursor);
    
    // Transparent background, standard border styling via Qt Style Sheets
    // while keeping auto-resizing based on text length
    setStyleSheet(R"(
        Button {
            background-color: #ffffff;
            color: rgb(137, 35, 137);
            border: 1.5px solid rgb(137, 35, 137);
            border-radius: 0px;
            padding: 4px 16px;
        }
        Button:hover {
            background-color: #fff2f8;
        }
        Button:pressed {
            background-color: #ffe0f0;
        }
    )");
}

void Button::paintEvent(QPaintEvent* event) {
    // Standard Qt stylesheet rendering works out of the box
    QPushButton::paintEvent(event);
}

QSize Button::sizeHint() const {
    QSize size = QPushButton::sizeHint();
    // Provide reasonable default padding width without strictly forcing a fixed width
    size.setWidth(qMax(size.width(), 120));
    size.setHeight(32);
    return size;
}

// ==========================================
// OverlayWidget Implementation (Backdrop Tint)
// ==========================================
OverlayWidget::OverlayWidget(QWidget* parent) : QWidget(parent) {
    if (parent) {
        setGeometry(parent->rect());
    }
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void OverlayWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    // Dark semi-transparent overlay to tint background
    painter.fillRect(rect(), QColor(0, 0, 0, 110)); 
}

// ==========================================
// Dialog Implementation
// ==========================================
Dialog::Dialog(const QString& title, 
                           const QString& message, 
                           const QString& detailText, 
                           DialogType type, 
                           QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Widget) 
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(450, 260);

    // Root layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, BOTTOM_BAR_HEIGHT);
    mainLayout->setSpacing(0);

    // 1. Title Bar
    QWidget* titleBar = new QWidget(this);
    titleBar->setFixedHeight(TITLE_BAR_HEIGHT);
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 6, 15, 0);

    QLabel* titleLabel = new QLabel(title, titleBar);
    titleLabel->setFont(QFont("Quicksand", 11, QFont::ExtraBold));
    titleLabel->setStyleSheet("color: rgb(137, 35, 137); background: transparent;");
    titleLayout->addWidget(titleLabel);

    mainLayout->addWidget(titleBar);

    // 2. Central Content Area
    QVBoxLayout* contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(25, 20, 25, 15);
    contentLayout->setAlignment(Qt::AlignCenter);

    QLabel* msgLabel = new QLabel(message, this);
    msgLabel->setFont(QFont("Quicksand", 12, QFont::Bold));
    msgLabel->setStyleSheet("color: rgb(137, 35, 137); background: transparent;");
    msgLabel->setAlignment(Qt::AlignCenter);
    msgLabel->setWordWrap(true);
    contentLayout->addWidget(msgLabel);

    if (!detailText.isEmpty()) {
        contentLayout->addSpacing(8);
        QLabel* detailLabel = new QLabel(detailText, this);
        detailLabel->setFont(QFont("Quicksand", 11, QFont::Bold));
        detailLabel->setStyleSheet("color: rgb(137, 35, 137); background: transparent;");
        detailLabel->setAlignment(Qt::AlignCenter);
        contentLayout->addWidget(detailLabel);
    }

    contentLayout->addStretch();

    // 3. Action Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(25);
    buttonLayout->setAlignment(Qt::AlignCenter);

    if (type == YesNo) {
        Button* btnYes = new Button("Yes", this);
        Button* btnNo  = new Button("No", this);

        connect(btnYes, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnNo,  &QPushButton::clicked, this, &QDialog::reject);

        buttonLayout->addWidget(btnYes);
        buttonLayout->addWidget(btnNo);
    } else { // Confirm (1 button)
        Button* btnOk = new Button("OK", this);
        connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
        buttonLayout->addWidget(btnOk);
    }

    contentLayout->addLayout(buttonLayout);
    mainLayout->addLayout(contentLayout);
}

void Dialog::paintEvent(QPaintEvent* /*event*/) {
    static const int titleBarHeight = 35;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(QRect{0,0,width(),titleBarHeight}, QColorConstants::White);

    // Main background vertical gradient
    QLinearGradient bgGradient(0, titleBarHeight, 0, height()-titleBarHeight-BOTTOM_BAR_HEIGHT);
    bgGradient.setColorAt(0, QColor("#fff7e7"));
    bgGradient.setColorAt(1, QColor("#ffffff"));
    painter.fillRect(QRect{0,titleBarHeight,width(),height()-BOTTOM_BAR_HEIGHT-titleBarHeight}, bgGradient);

    // Pink bottom bar horizontal gradient
    QLinearGradient bottomGradient(0, 0, width(), 0);
    bottomGradient.setColorAt(0, QColor("#f28bc1"));
    bottomGradient.setColorAt(1, QColor("#f38ac3"));

    painter.setBrush(bottomGradient);
    painter.setPen(Qt::NoPen);

    int rectHeight = BOTTOM_BAR_HEIGHT;
    int rectY = height() - rectHeight;
    painter.drawRect(0, rectY, width(), rectHeight);
}

// Static function handling tinted background logic automatically
bool Dialog::showDialog(QWidget* parent, 
                              const QString& title, 
                              const QString& message, 
                              const QString& detailText, 
                              DialogType type) 
{
    OverlayWidget* overlay = nullptr;
    if (parent) {
        overlay = new OverlayWidget(parent);
        overlay->resize(parent->size());
        overlay->show();
    }

    Dialog dlg(title, message, detailText, type, parent);
    if (parent) {
        // Center dialog over parent window
        dlg.move(parent->geometry().center() - dlg.rect().center());
    }

    int result = dlg.exec();

    if (overlay) {
        overlay->deleteLater();
    }

    return (result == QDialog::Accepted);
}
