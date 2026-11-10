#include "Router.hpp"
#include <QVBoxLayout>

Router::Router(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_stack = new QStackedWidget(this);
  layout->addWidget(m_stack);
}

void Router::addRoute(const QString &pattern,
                      RouteFactory factory,
                      TransitionHandler transition) {
  m_routes.append({pattern, factory, transition});
}

void Router::addFixedRoute(const QString &path,
                           QWidget *widget,
                           TransitionHandler transition) {
  m_routes.append({path, [widget](const RouteParams&) { return widget; }, transition, true});
}

void Router::navigate(const QString &path) {
  if (path == m_currentPath)
    return;

  // Find route match
  QStringList targetSegments = path.split('/', Qt::SkipEmptyParts);
  RouteMatch match = matchRoute(targetSegments);

  if (!match.success) {
    qWarning() << "Route not found:" << path;
    return;
  }

  QWidget *incomingWidget = match.definition.factory(match.params);
  if (!incomingWidget)
    return;

  m_currentPath = path;

  // Prepare stack
  incomingWidget->setParent(m_stack);
  m_stack->addWidget(incomingWidget);

  QWidget *outgoingWidget = m_stack->currentWidget();
  // Temporarily set incoming as current so layout updates,
  // but let transition handler manage the visual state swap.
  m_stack->setCurrentWidget(incomingWidget);

  if (!outgoingWidget || outgoingWidget == incomingWidget) {
    return; // First widget or same, no transition needed
  }

  auto transitionHandler = match.definition.transition;
  bool doDelete = !match.definition.fixedPath;
  incomingWidget->setProperty("__mvcr_delete", doDelete);
  if (transitionHandler) {
    transitionHandler(path, outgoingWidget, incomingWidget, [this, outgoingWidget]() {
      m_stack->removeWidget(outgoingWidget);
      if (outgoingWidget->property("__mvcr_delete").toBool()) outgoingWidget->deleteLater();
    });
  }
}

Router::RouteMatch Router::matchRoute(const QStringList &targetSegments) {
  for (const auto &route : m_routes) {
    QStringList patternSegments = route.pattern.split('/', Qt::SkipEmptyParts);
    if (patternSegments.size() != targetSegments.size())
      continue;

    RouteParams params;
    bool matched = true;

    for (int i = 0; i < patternSegments.size(); ++i) {
      QString pSeg = patternSegments[i];
      QString tSeg = targetSegments[i];

      if (pSeg.startsWith(':')) {
        params[pSeg.mid(1)] = tSeg;
      } else if (pSeg != tSeg) {
        matched = false;
        break;
      }
    }

    if (matched) {
      return {true, params, route};
    }
  }
  return {false, {}, {}};
}
