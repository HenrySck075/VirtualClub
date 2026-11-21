#include "Home.hpp"
#include <QLayout>
#include <QLabel>
#include <QFrame>
#include <QMouseEvent>
#include <functional>
#include "ui/ElidedLabel.hpp"
#include "ui/GradientBackground.hpp"
#include "ui/Router.hpp"
#include "consts.hpp"
#include "utils/FlowLayout.hpp"
#include "utils/ModIndex.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/AutoUpdater.hpp"
#include "ui/MESWidgets.hpp"
#include "utils/LucideIcons.hpp"
#include <QHBoxLayout>
#include <QPushButton>

class RecentModCard : public QFrame {
  Q_OBJECT
public:
  RecentModCard(const ModsIndex::Mod& mod, std::function<void()> onClicked,
                QWidget* parent = nullptr)
    : QFrame(parent), m_onClicked(std::move(onClicked)) {
    setObjectName("recentModCard");
    setStyleSheet(
      "#recentModCard {"
      "  background-color: rgba(255, 255, 255, 35);"
      "  border: 1px solid rgba(255, 255, 255, 55);"
      "  border-radius: 8px;"
      "}"
    );
    setFixedWidth(150);
    setCursor(Qt::PointingHandCursor);

    auto* cardLayout = new QVBoxLayout(this);
    cardLayout->setContentsMargins(10, 10, 10, 8);
    cardLayout->setSpacing(4);

    auto* icon = new QLabel(this);
    icon->setFixedSize(128, 96);
    icon->setAlignment(Qt::AlignCenter);
    const auto pixmap = QPixmap(QString::fromStdString(mod.getIconPath()));
    if (!pixmap.isNull()) {
      icon->setPixmap(pixmap.scaled(
        icon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation
      ));
    }
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(icon);

    auto* name = new ElidedLabel(QString::fromStdString(mod.name), this);
    name->setFont(QFont("Quicksand", 11, QFont::Weight::DemiBold));
    //name->setStyleSheet("color: ;");
    name->setAlignment(Qt::AlignCenter);
    name->setMaxLines(2);
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(name);

    auto* version = new QLabel(
      tr("Version: %1").arg(QString::fromStdString(mod.version)), this
    );
    version->setFont(QFont("Quicksand", 9));
    //version->setStyleSheet("color: rgba(255, 255, 255, 180);");
    version->setAlignment(Qt::AlignCenter);
    version->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(version);
  }

protected:
  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton && m_onClicked) {
      m_onClicked();
      event->accept();
      return;
    }
    QFrame::mouseReleaseEvent(event);
  }

private:
  std::function<void()> m_onClicked;
};

HomeScreen::HomeScreen(Router* router, QWidget* parent)
  : QWidget(parent), m_router(router), m_cards(nullptr), m_cardsLayout(nullptr) {
  // Set up the layout for the Home screen
  auto* layout = new QVBoxLayout(this);
  static const int margin = 24;
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(4);

  auto* header = new QLabel(
    QString("wassup %1").arg(
      ProfileSettings::get()->value(STK_DISPLAYNAME).toString()
    ),
    this
  );
  header->setStyleSheet("color: white; font-size: 25px; font-weight: bold;");
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  layout->addWidget(header);
  auto* content = new QWidget(this);
  setupGradientWidget(content);
  layout->addWidget(content);

  auto* contentLayout = new QVBoxLayout(content);
  static const int contentMargin = 16;
  contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  contentLayout->setSpacing(0);
  contentLayout->setAlignment(Qt::AlignTop);

  m_updater = new AutoUpdater(this);

  m_updateRow = new QFrame(content);
  m_updateRow->setObjectName("updateRow");
  m_updateRow->setStyleSheet(
      "#updateRow {"
      " background-color: rgba(137, 35, 137, 45);"
      " border: 1px solid rgba(137, 35, 137, 120);"
      " border-radius: 8px;"
      "}"
  );
  m_updateRow->setHidden(true);
  auto* updateRowLayout = new QHBoxLayout(m_updateRow);
  updateRowLayout->setContentsMargins(12, 8, 12, 8);
  m_updateLabel = new QLabel(m_updateRow);
  m_updateLabel->setWordWrap(true);
  updateRowLayout->addWidget(m_updateLabel, 1);
  auto* installButton = new Button(tr("Update"), LucideIcons::download, m_updateRow);
  updateRowLayout->addWidget(installButton, 0, Qt::AlignRight);
  contentLayout->addWidget(m_updateRow);
  connect(installButton, &QPushButton::clicked, this, [this]() {
    if (!m_updateManifest.empty()) {
      m_updater->downloadUpdate(m_updateManifest);
    }
  });

  {
    auto* label = new QLabel("Recently played", content);
    label->setFont(QFont("Quicksand", 18, QFont::Weight::Bold));
    contentLayout->addWidget(label);
  }
  m_cards = new QWidget(content);
  m_cards->setFixedHeight(170);
  m_cardsLayout = new FlowLayout(m_cards);
  m_cardsLayout->setContentsMargins(0, 4, 0, 4);
  m_cardsLayout->setSpacing(12);
  m_cardsLayout->setAlignment(Qt::AlignLeft);
  contentLayout->addWidget(m_cards);

  updateRecentlyPlayed();
  connect(ModsIndex::get().get(), &ModsIndex::recentlyPlayedUpdated,
          this, &HomeScreen::updateRecentlyPlayed);
}

void HomeScreen::showUpdateAvailable(const nlohmann::json& manifest) {
  m_updateManifest = manifest;
  const QString version = QString::fromStdString(
      manifest.value("version", ""));
  m_updateLabel->setText(tr("Update available: VirtualClub %1").arg(version));
  m_updateRow->setHidden(false);
}

void HomeScreen::updateRecentlyPlayed() {
  while (m_cardsLayout->count() > 0) {
    auto* item = m_cardsLayout->takeAt(0);
    delete item->widget();
    delete item;
  }

  const auto& recentlyPlayed = ModsIndex::get()->getRecentlyPlayed();
  if (recentlyPlayed.empty()) {
    auto* emptyLabel = new QLabel(tr("No recently played mods yet."), m_cards);
    emptyLabel->setFont(QFont("Quicksand", 12));
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_cardsLayout->addWidget(emptyLabel);
    return;
  }

  for (const auto& mod : recentlyPlayed) {
    const auto modId = QString::fromStdString(mod.id);
    auto* card = new RecentModCard(
      mod,
      [router = m_router, modId] { router->navigate("/mods/" + modId); },
      m_cards
    );
    m_cardsLayout->addWidget(card);
  }
}

#include "Home.moc"
