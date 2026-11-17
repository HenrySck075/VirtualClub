#include "Home.hpp"
#include <QLayout>
#include <QLabel>
#include <QFrame>
#include <QMouseEvent>
#include <functional>
#include "ui/GradientBackground.hpp"
#include "ui/Router.hpp"
#include "consts.hpp"
#include "utils/FlowLayout.hpp"
#include "utils/ModIndex.hpp"
#include "utils/ProfileSettings.hpp"

class RecentModCard : public QFrame {
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

    auto* name = new QLabel(QString::fromStdString(mod.name), this);
    name->setFont(QFont("Quicksand", 11, QFont::Weight::DemiBold));
    //name->setStyleSheet("color: ;");
    name->setAlignment(Qt::AlignCenter);
    name->setWordWrap(true);
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

  {
    auto* label = new QLabel("Recently played", content);
    label->setFont(QFont("Quicksand", 18, QFont::Weight::Bold));
    contentLayout->addWidget(label);
  }
  {
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
    emptyLabel->setStyleSheet("color: rgba(255, 255, 255, 180);");
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
