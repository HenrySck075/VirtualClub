#pragma once

#include "utils/ModIndex.hpp"
#include <memory>
#include <QString>

class VFSInstance {
public:
    VFSInstance(const QString& launcherRoot, const ModsIndex::Mod& mod);
    ~VFSInstance();

    VFSInstance(const VFSInstance&) = delete;
    VFSInstance& operator=(const VFSInstance&) = delete;
    VFSInstance(VFSInstance&&) = delete;
    VFSInstance& operator=(VFSInstance&&) = delete;

    bool mount();
    void unmount();
    QString mountPath() const;

#ifdef MVC_VFS_AVAILABLE
private:
    class Private;
    std::unique_ptr<Private> d;
#endif
};
