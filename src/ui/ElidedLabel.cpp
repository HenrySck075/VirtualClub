#include "ElidedLabel.hpp"

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QFrame(parent), m_text(text), m_maxLines(2), m_alignment(Qt::AlignLeft) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ElidedLabel::setText(const QString &text) {
  if (m_text != text) {
    m_text = text;
    update();
    updateGeometry();
  }
}

void ElidedLabel::setMaxLines(int lines) {
  if (m_maxLines != lines) {
    m_maxLines = lines;
    update();
    updateGeometry();
  }
}

void ElidedLabel::setAlignment(Qt::Alignment alignment) {
  if (m_alignment != alignment) {
    m_alignment = alignment;
    update();
  }
}

QSize ElidedLabel::minimumSizeHint() const {
  QFontMetrics fm(font());
  return QSize(50, fm.lineSpacing());
}

void ElidedLabel::resizeEvent(QResizeEvent *event) {
  QFrame::resizeEvent(event);
  // Width changed, which might change the number of wrapped lines and height
  updateGeometry();
}

void ElidedLabel::paintEvent(QPaintEvent *event) {
  QFrame::paintEvent(event);
  QPainter painter(this);
  painter.setFont(font());
  painter.setPen(palette().color(QPalette::WindowText));

  QFontMetrics fontMetrics = painter.fontMetrics();
  int lineSpacing = fontMetrics.lineSpacing();
  int y = 0;

  QTextLayout textLayout(m_text, font());
  textLayout.beginLayout();

  int lineCount = 0;
  while (true) {
    QTextLine line = textLayout.createLine();
    if (!line.isValid())
      break;

    line.setLineWidth(width());

    // Compute horizontal offset based on alignment
    qreal x = 0;
    qreal lineWidth = line.naturalTextWidth();
    
    if (m_alignment & Qt::AlignRight) {
      x = width() - lineWidth;
    } else if (m_alignment & Qt::AlignHCenter) {
      x = (width() - lineWidth) / 2.0;
    }

    if (lineCount < m_maxLines - 1) {
      line.draw(&painter, QPointF(x, y));
      y += lineSpacing;
      lineCount++;
    } else {
      QString remainingText = m_text.mid(line.textStart());
      QString elidedText =
          fontMetrics.elidedText(remainingText, Qt::ElideRight, width());

      qreal elidedWidth = fontMetrics.horizontalAdvance(elidedText);
      qreal elidedX = 0;
      if (m_alignment & Qt::AlignRight) {
        elidedX = width() - elidedWidth;
      } else if (m_alignment & Qt::AlignHCenter) {
        elidedX = (width() - elidedWidth) / 2.0;
      }

      painter.drawText(QPointF(elidedX, y + fontMetrics.ascent()), elidedText);
      break;
    }
  }
  textLayout.endLayout();
}

QSize ElidedLabel::calculateSize(int w) const {
  if (w <= 0) {
    w = 250; // Fallback width if layout hasn't initialized width yet
  }

  QFontMetrics fm(font());
  int lineSpacing = fm.lineSpacing();

  QTextLayout textLayout(m_text, font());
  textLayout.beginLayout();

  int lineCount = 0;
  while (true) {
    QTextLine line = textLayout.createLine();
    if (!line.isValid())
      break;
    line.setLineWidth(w);
    lineCount++;

    // Cap counting at maxLines for height computation
    if (lineCount >= m_maxLines) {
      break;
    }
  }
  textLayout.endLayout();

  return QSize(w, lineSpacing * qMax(1, lineCount));
}

