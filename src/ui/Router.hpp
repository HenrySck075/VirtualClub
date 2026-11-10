#pragma once  

#include <QMap>
#include <QString>
#include <functional>
#include <QWidget>
#include <QStackedWidget>

using RouteParams = QMap<QString, QString>;

// Transition signature: takes outgoing widget, incoming widget, and a completion callback
using TransitionHandler = std::function<void(QString path, QWidget* outgoing, QWidget* incoming, std::function<void()> onFinished)>;
using RouteFactory = std::function<QWidget* (const RouteParams&)>;

/// Mininal enough router widget to display and "navigate between" multiple widgets.
/// Handles "/path/:like/this/" but thats it
class Router : public QWidget {
    Q_OBJECT
public:
  explicit Router(QWidget* parent = nullptr);

  void addRoute(const QString& pattern,
                RouteFactory factory,
                TransitionHandler transition = nullptr);
  void addFixedRoute(const QString& path,
                     QWidget* widget,
                     TransitionHandler transition = nullptr);

public slots:
  void navigate(const QString &path);

private:
    struct RouteDefinition {
        QString pattern;
        RouteFactory factory;
        TransitionHandler transition;
        // this is just to determine if i should call deleteLater
        // on outgoing transition finished
        bool fixedPath = false;
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
