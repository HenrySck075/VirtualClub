#ifndef LucideIcons_H
#define LucideIcons_H

#include <QIcon>
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
    house,
    library,
    plus,
    settings,
  )
};

#endif
