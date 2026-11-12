#include "ImageLoadingWidget.hpp"

#include "LoadingCircle.hpp"
#include "PixmapWidget.hpp"
#include "utils/utils.hpp"

#include <QNetworkAccessManager>
#include <QStackedWidget>
#include <QVBoxLayout>

ImageLoadingWidget::ImageLoadingWidget(const QString& imageUrl,
                                       QNetworkAccessManager* networkManager,
                                       QWidget* parent)
    : QWidget(parent), m_nm(networkManager), m_imageUrl(imageUrl) {
  m_container = new QStackedWidget(this);
  m_container->setGeometry(rect());

  auto* placeholderWidget = new QWidget();
  placeholderWidget->setStyleSheet("background-color: #dddddd;");
  m_container->addWidget(placeholderWidget);

  auto* placeholderLayout = new QVBoxLayout(placeholderWidget);
  placeholderLayout->setContentsMargins(0, 0, 0, 0);

  if (!imageUrl.isEmpty()) {
    auto* loadingCircle = new LoadingCircle(placeholderWidget);
    loadingCircle->setColor(Qt::white);
    placeholderLayout->addWidget(loadingCircle, 0, Qt::AlignCenter);
  }
}

void ImageLoadingWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  m_container->setGeometry(rect());
}


void ImageLoadingWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!m_imageUrl.isEmpty() && !m_requestMade) {
    m_requestMade = true;
    fetchUrlAsync(m_nm, QUrl(m_imageUrl)).then(this, [this](QByteArray data) {
      QPixmap pixmap;
      if (pixmap.loadFromData(data)) {
        auto* pixmapWidget = new PixmapWidget(this);
        pixmapWidget->setPixmap(pixmap);
        m_container->addWidget(pixmapWidget);
        m_container->setCurrentWidget(pixmapWidget);
      } else {
        qWarning("Failed to load image from data.");
      }
    });
  }
}
