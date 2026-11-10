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

// Contains a small subset of the Lucide Icons library used by the app. 
// New icons may be added over time if needed.
//
// There used to be a macro that hides the repetition 
// but it caused qt6.12's moc to segfault due to the compiler's bug
namespace LucideIcons {
inline static LazyIcon arrow_left{":/icons/arrow_left.svg"};
inline static LazyIcon bug{":/icons/bug.svg"};
inline static LazyIcon chevron_left{":/icons/chevron_left.svg"};
inline static LazyIcon chevron_right{":/icons/chevron_right.svg"};
inline static LazyIcon chevrons_left{":/icons/chevrons_left.svg"};
inline static LazyIcon chevrons_right{":/icons/chevrons_right.svg"};
inline static LazyIcon clock{":/icons/clock.svg"};
inline static LazyIcon file_symlink{":/icons/file_symlink.svg"};
inline static LazyIcon folder{":/icons/folder.svg"};
inline static LazyIcon folder_open_dot{":/icons/folder_open_dot.svg"};
inline static LazyIcon folder_pen{":/icons/folder_pen.svg"};
inline static LazyIcon globe{":/icons/globe.svg"};
inline static LazyIcon house{":/icons/house.svg"};
inline static LazyIcon library{":/icons/library.svg"};
inline static LazyIcon loader_circle{":/icons/loader_circle.svg"};
inline static LazyIcon pen{":/icons/pen.svg"};
inline static LazyIcon play{":/icons/play.svg"};
inline static LazyIcon plus{":/icons/plus.svg"};
inline static LazyIcon refresh_cw{":/icons/refresh_cw.svg"};
inline static LazyIcon rotate_cw_clock{":/icons/rotate_cw_clock.svg"};
inline static LazyIcon settings{":/icons/settings.svg"};
inline static LazyIcon timer{":/icons/timer.svg"};
inline static LazyIcon trash{":/icons/trash.svg"};
inline static LazyIcon users{":/icons/users.svg"};
inline static LazyIcon x{":/icons/x.svg"};
};

#endif
