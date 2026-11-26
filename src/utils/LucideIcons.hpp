#ifndef LucideIcons_H
#define LucideIcons_H

#include <QColor>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <utility>
#include "macros.h"

class LucideIcon {
    QString m_path;

public:
    explicit LucideIcon(QString path) : m_path(std::move(path)) {}

    QIcon icon() const {
        return QIcon(m_path);
    }

    operator QIcon() const {
        return icon();
    }

    QPixmap pixmap(const QSize &size = QSize(), const QColor &color = Qt::black) const {
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly))
            return {};

        QByteArray svg = file.readAll();
        svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
        QSvgRenderer renderer(svg);
        if (!renderer.isValid())
            return {};

        const QSize logicalSize = size.isValid() && !size.isEmpty()
            ? size
            : renderer.defaultSize();
        const qreal devicePixelRatio = 1.0;
        QPixmap result(logicalSize);
        result.setDevicePixelRatio(devicePixelRatio);
        result.fill(Qt::transparent);

        QPainter painter(&result);
        renderer.render(&painter, QRectF(QPointF(0, 0), logicalSize));
        return result;
    }
};
#pragma region scary macros

#define LI_DefineIcons_INDIRECT() LI_DefineIcons
#define LI_DefineIcons(icon, ...) \
  inline static const LucideIcon icon {":/icons/"#icon ".svg"}; \
  __VA_OPT__(DEFER1(LI_DefineIcons_INDIRECT)()(__VA_ARGS__))
#define LucideIcons_IMPL(...) \
  EVAL(LI_DefineIcons(__VA_ARGS__))

#pragma endregion
namespace LucideIcons {
  LucideIcons_IMPL(
    arrow_left,
    arrow_right,
    bug,
    calendar,
    chevron_left,
    chevron_right,
    chevrons_left,
    chevrons_right,
    clock,
    download,
    file_symlink,
    folder,
    folder_open_dot,
    folder_pen,
    globe,
    house,
    image_up,
    library,
    link,
    loader_circle,
    pen,
    play,
    plus,
    refresh_cw,
    rotate_cw_clock,
    settings,
    timer,
    trash,
    users,
    x,
  )
};

#endif
