#ifndef LucideIcons_H
#define LucideIcons_H

#include <QColor>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <optional>
#include <utility>

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

namespace LucideIcons {
inline static const LucideIcon arrow_left{":/icons/arrow_left.svg"};
inline static const LucideIcon arrow_right{":/icons/arrow_right.svg"};
inline static const LucideIcon bug{":/icons/bug.svg"};
inline static const LucideIcon chevron_left{":/icons/chevron_left.svg"};
inline static const LucideIcon chevron_right{":/icons/chevron_right.svg"};
inline static const LucideIcon chevrons_left{":/icons/chevrons_left.svg"};
inline static const LucideIcon chevrons_right{":/icons/chevrons_right.svg"};
inline static const LucideIcon clock{":/icons/clock.svg"};
inline static const LucideIcon download{":/icons/download.svg"};
inline static const LucideIcon file_symlink{":/icons/file_symlink.svg"};
inline static const LucideIcon folder{":/icons/folder.svg"};
inline static const LucideIcon folder_open_dot{":/icons/folder_open_dot.svg"};
inline static const LucideIcon folder_pen{":/icons/folder_pen.svg"};
inline static const LucideIcon globe{":/icons/globe.svg"};
inline static const LucideIcon house{":/icons/house.svg"};
inline static const LucideIcon library{":/icons/library.svg"};
inline static const LucideIcon loader_circle{":/icons/loader_circle.svg"};
inline static const LucideIcon pen{":/icons/pen.svg"};
inline static const LucideIcon play{":/icons/play.svg"};
inline static const LucideIcon plus{":/icons/plus.svg"};
inline static const LucideIcon refresh_cw{":/icons/refresh_cw.svg"};
inline static const LucideIcon rotate_cw_clock{":/icons/rotate_cw_clock.svg"};
inline static const LucideIcon settings{":/icons/settings.svg"};
inline static const LucideIcon timer{":/icons/timer.svg"};
inline static const LucideIcon trash{":/icons/trash.svg"};
inline static const LucideIcon users{":/icons/users.svg"};
inline static const LucideIcon x{":/icons/x.svg"};
};

#endif
