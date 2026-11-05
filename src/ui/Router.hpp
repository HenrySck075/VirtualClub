#include <QMap>
#include <QString>
#include <functional>
#include <QWidget>
#include <QStackedWidget>

using RouteParams = QMap<QString, QString>;

// Transition signature: takes outgoing widget, incoming widget, and a completion callback
using TransitionHandler = std::function<void(QWidget *outgoing, QWidget *incoming, std::function<void()> onFinished)>;
using RouteFactory = std::function<QWidget* (const RouteParams&)>;

/// Mininal enough router widget to display and "navigate between" multiple widgets.
/// Handles "/path/:like/this/" but thats it
class Router : public QWidget {
    Q_OBJECT
public:
  explicit Router(QWidget *parent = nullptr);

  void addRoute(const QString &pattern,
                RouteFactory factory,
                TransitionHandler transition = nullptr);

public slots:
  void navigate(const QString &path);

private:
    struct RouteDefinition {
        QString pattern;
        RouteFactory factory;
        TransitionHandler transition;
    };

    struct RouteMatch {
        bool success = false;
        RouteParams params;
        RouteDefinition definition;
    };

    QList<RouteDefinition> m_routes;
    QStackedWidget *m_stack;
    QString m_currentPath;

    RouteMatch matchRoute(const QStringList &targetSegments);
};
