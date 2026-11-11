#pragma once

#include <QFrame>
#include <QString>
#include <QTextLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>

class ElidedLabel : public QFrame {
  Q_OBJECT
  Q_PROPERTY(QString text READ text WRITE setText)

public:
  explicit ElidedLabel(const QString &text, QWidget *parent = nullptr);

  void setText(const QString &text);

  QString text() const { return m_text; }

  void setMaxLines(int lines);

  int maxLines() const { return m_maxLines; }

  // Dynamically calculate height based on the current width and line count
  QSize sizeHint() const override { return calculateSize(width()); }

  QSize minimumSizeHint() const override;

protected:
  void resizeEvent(QResizeEvent *event) override;

  void paintEvent(QPaintEvent *event) override;

private:
  QSize calculateSize(int w) const;

  QString m_text;
  int m_maxLines = 2;
};
