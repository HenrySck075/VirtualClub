#pragma once

#include <QWidget>

class QNetworkAccessManager;
class QStackedWidget;

class ImageLoadingWidget : public QWidget {
  Q_OBJECT
public:
  explicit ImageLoadingWidget(const QString& imageUrl,
                              QNetworkAccessManager* networkManager,
                              QWidget* parent = nullptr);

private:
  QStackedWidget* m_container = nullptr;

protected:
  void resizeEvent(QResizeEvent* event) override;
};
