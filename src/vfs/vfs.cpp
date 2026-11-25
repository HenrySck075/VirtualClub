#include "vfs.hpp"

#include <stdexcept>

#if !defined(MVC_VFS_AVAILABLE)

VFSInstance::VFSInstance(const QString&, const ModsIndex::Mod&) {
  throw std::runtime_error("VFS is not available on this platform.");
}

VFSInstance::~VFSInstance() = default;

bool VFSInstance::mount() {
  return false;
}

void VFSInstance::unmount() {}

QString VFSInstance::mountPath() const {
  return {};
}

#endif
