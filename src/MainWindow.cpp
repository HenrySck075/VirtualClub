#include <QPainter>
#include <QLayout>
#include <QScrollArea>
#include <QEvent>
#include <QCoreApplication>
#include <QTime>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <qapplication.h>
#include <qtypes.h>
#include "MainWindow.hpp"
#include "screens/Home.hpp"
#include "screens/Mods.hpp"
#include "screens/Settings.hpp"
#include "ui/Sidebar.hpp"
#include "ui/CoverImageWidget.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/anime.hpp"

// Helper to parse times like "18", "18.30", "6.15", "6" into QTime
QTime parseFlexibleTime(const std::string& str) {
    QString qstr = QString::fromStdString(str).trimmed();
    QTime t = QTime::fromString(qstr, "H.m");
    if (!t.isValid()) {
        t = QTime::fromString(qstr, "H");
    }
    return t;
}

// Checks if current time falls within [start, end], handling midnight wraps
bool isTimeInRange(const QTime& current, const QTime& start, const QTime& end) {
    if (start <= end) {
        // Standard range within the same day (e.g., 08:00 to 16:30)
        return current >= start && current <= end;
    } else {
        // Overnight range wrapping past midnight (e.g., 18:00 to 06:00)
        return current >= start || current <= end;
    }
}

struct BgImageCandidate {
    QTime startTime;
    QTime endTime;
    std::filesystem::path path;
};

QImage getCurrentImage(const std::string& pack) {
    namespace fs = std::filesystem;

    fs::path bgAssetsFolder = fs::path(QCoreApplication::applicationDirPath().toStdString()) 
                            / "assets" / "bg" / pack;
    std::cout << bgAssetsFolder << " " << std::endl;

    std::error_code ec;
    if (!fs::exists(bgAssetsFolder, ec) || !fs::is_directory(bgAssetsFolder, ec)) {
        std::cerr << "ow" << std::endl;
        return QImage(); // Empty QImage indicates failure
    }

    std::vector<BgImageCandidate> candidates;

    // Collect and parse files
    for (const auto& entry : fs::directory_iterator(bgAssetsFolder, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().stem().string(); // filename without extension
        size_t dashIdx = filename.find('-');
        if (dashIdx == std::string::npos) continue;

        std::string startStr = filename.substr(0, dashIdx);
        std::string endStr = filename.substr(dashIdx + 1);

        QTime startTime = parseFlexibleTime(startStr);
        QTime endTime = parseFlexibleTime(endStr);

        if (startTime.isValid() && endTime.isValid()) {
            candidates.push_back({startTime, endTime, entry.path()});
        }
    }

    // Sort in reverse numerical order based on start time (e.g., 20:00 before 18:00 before 06:00)
    std::sort(candidates.begin(), candidates.end(), [](const BgImageCandidate& a, const BgImageCandidate& b) {
        return a.startTime > b.startTime;
    });

    QTime currentTime = QTime::currentTime();

    // Iterate through candidates (now sorted in reverse numerical order)
    for (const auto& item : candidates) {
        if (isTimeInRange(currentTime, item.startTime, item.endTime)) {
            std::cout << "Selected image: " << item.path << " (Current time: " << currentTime.toString().toStdString() << ")" << std::endl;
            return QImage(QString::fromStdString(item.path.string()));
        }
    }

    return QImage(); // Return null/empty QImage if no matching interval matches
}

class ChildResizerFilter : public QObject {
    Q_OBJECT
public:
    ChildResizerFilter(QWidget *child, QObject *parent = nullptr)
        : QObject(parent), m_child(child) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Resize) {
            auto *parentWidget = qobject_cast<QWidget*>(watched);
            if (parentWidget && m_child) {
                // Keep child sized to match parent
                m_child->resize(parentWidget->size());
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *m_child;
};

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  setWindowTitle("VirtualClub Ren'Py Mod Manager");
  resize(1020, 600); // Set default starting size (Width, Height)

  m_currentBackgroundImage = getCurrentImage(m_settings.currentBackground);
  initUI();
}

void MainWindow::initUI() {
  auto *tossaway = new QVBoxLayout(this);
  tossaway->setContentsMargins(0, 0, 0, 0);

  m_rootWidget = new QWidget(this);
  m_rootWidget->setContentsMargins(0, 0, 0, MainWindow::BOTTOM_BAR_HEIGHT); // Bottom padding for the bar
  tossaway->addWidget(m_rootWidget);
  auto *mainLayout = new QHBoxLayout(m_rootWidget);
  mainLayout->setAlignment(Qt::AlignmentFlag::AlignLeft);
  mainLayout->setContentsMargins(0, 0, 0, 0); // Spacing inside the layout itself
  mainLayout->setSpacing(0);

  // sidebar
  m_sidebar = new Sidebar(m_rootWidget);
  mainLayout->addWidget(m_sidebar);

  m_mainContent = new CoverImageWidget(m_rootWidget);
  auto *contentLayout = new QVBoxLayout(m_mainContent);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  m_stackedWidget = new QStackedWidget(m_mainContent);
  contentLayout->addWidget(m_stackedWidget);

  static_cast<CoverImageWidget*>(m_mainContent)->setPixmap(QPixmap::fromImage(m_currentBackgroundImage));
  // make m_mainContent expands to the remaining portion of the layout
  m_mainContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mainLayout->addWidget(m_mainContent);

  addNavigationItem(LucideIcons::house, "Home", SidebarPosition::Top, new HomeScreen())->setSelected(true);
  addNavigationItem(LucideIcons::library, "Mods", SidebarPosition::Top, new ModsScreen());

  addNavigationItem(LucideIcons::settings, "Settings", SidebarPosition::Bottom, new SettingsScreen());

  connect(m_sidebar, &Sidebar::selectedItemChanged, this, &MainWindow::onSelectedItemChanged);

  m_rootWidget->show();
}

void MainWindow::onSelectedItemChanged(SidebarItem* oldItem, SidebarItem* newItem) {
  int oidx = oldItem ? m_sidebar->itemPositionOf(oldItem) : -1;
  int nidx = m_sidebar->itemPositionOf(newItem);

  bool ttbDirection = oidx > nidx; // top-to-bottom if true (oldItem pos > newItem pos), else bottom-to-top

  auto thisWidget = m_navigationMap[newItem];
  m_stackedWidget->setCurrentWidget(thisWidget);

  anime::slideFade(thisWidget, ttbDirection ? anime::SlideDirection::Down : anime::SlideDirection::Up); 
}

SidebarItem* MainWindow::addNavigationItem(QIcon icon, std::string name, SidebarPosition position, QWidget* widget) {
  auto item = m_sidebar->addSidebarItem(icon, name, position);

  m_navigationMap[item] = widget;
  m_stackedWidget->addWidget(widget);
  widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

  return item;
};

void MainWindow::paintEvent(QPaintEvent *event) {
  // Pass 'this' so the QPainter targets this specific QWidget window
  QPainter painter(this);

  QLinearGradient bottomGradient(0, 0, width(), 0);
  bottomGradient.setColorAt(0, QColor("#f28bc1"));
  bottomGradient.setColorAt(1, QColor("#f38ac3"));
  // Set brush color to pink (RGB or hex) with no border line
  painter.setBrush(bottomGradient); // Hot Pink
  painter.setPen(Qt::NoPen);

  // Calculate dynamic dimensions for the rectangle at the bottom
  int rectHeight = MainWindow::BOTTOM_BAR_HEIGHT;
  int rectY = height() - rectHeight; // Position at the bottom edge

  // Draw rectangle: x, y, width, height
  // width() automatically reflects the current window width
  painter.drawRect(0, rectY, width(), rectHeight);
};
MainWindow *getMainWindow() {
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (auto *mainWin = qobject_cast<MainWindow *>(widget)) {
      return mainWin;
    }
  }
  return nullptr;
}
#include "MainWindow.moc"
