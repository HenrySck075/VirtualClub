#ifndef LucideIcons_H
#define LucideIcons_H

#include <QIcon>
#include <qpixmap.h>
#include "macros.h"
// workaround for the uhh
class LazyIcon {
    QString m_path;
    mutable std::unique_ptr<QIcon> m_icon;
public:
    LazyIcon(QString path) : m_path(std::move(path)) {}
    
    // Implicit conversion to QIcon!
    operator const QIcon&() const {
        if (!m_icon) {
            m_icon = std::make_unique<QIcon>(m_path);
        }
        return *m_icon;
    }

    // some forwarding functions from QIcon because it wasn't as implicit as they thought.
    const QPixmap pixmap(const QSize &size = QSize(), QIcon::Mode mode = QIcon::Normal, QIcon::State state = QIcon::Off) const {
        return static_cast<const QIcon&>(*this).pixmap(size, mode, state);
    }

};
#pragma region scary macros

#define LI_DefineIcons_INDIRECT() LI_DefineIcons
#define LI_DefineIcons(icon, ...) \
  inline static LazyIcon icon {":/icons/"#icon ".svg"}; \
  __VA_OPT__(DEFER1(LI_DefineIcons_INDIRECT)()(__VA_ARGS__))
#define LucideIcons_IMPL(...) \
  EVAL(LI_DefineIcons(__VA_ARGS__))

#pragma endregion

namespace LucideIcons {
  LucideIcons_IMPL(
    arrow_left,
    bug,
    folder,
    folder_pen,
    house,
    library,
    play,
    plus,
    trash,
    refresh_cw,
    rotate_cw_clock,
    settings,
    users,
    x,
  )
};

#endif
