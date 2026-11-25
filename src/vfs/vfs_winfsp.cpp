#include "utils/macros.h"
#include "vfs.hpp"
#include "vfs_core.hpp"

#if defined(MVC_VFS_AVAILABLE) && defined(Q_OS_WIN)

#include <windows.h>
#include <winfsp/winfsp.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <QDebug>
#include <QDir>
#include <QTemporaryDir>

namespace {

constexpr std::int64_t WindowsEpochOffset = 11644473600LL;
constexpr std::uint64_t WindowsTicksPerSecond = 10000000ULL;

struct FileContext {
  vfs::HandleContext handle;
  std::string path;
  bool isDirectory = false;
  bool cleaned = false;
};

NTSTATUS winfspStatus(vfs::Status status) {
  switch (status) {
  case vfs::Status::Ok: return STATUS_SUCCESS;
  case vfs::Status::NoSuchEntry: return STATUS_OBJECT_NAME_NOT_FOUND;
  case vfs::Status::EntryExists: return STATUS_OBJECT_NAME_COLLISION;
  case vfs::Status::NotDirectory: return STATUS_NOT_A_DIRECTORY;
  case vfs::Status::IsDirectory: return STATUS_FILE_IS_A_DIRECTORY;
  case vfs::Status::PermissionDenied:
  case vfs::Status::OperationNotPermitted:
    return STATUS_ACCESS_DENIED;
  case vfs::Status::DirectoryNotEmpty: return STATUS_DIRECTORY_NOT_EMPTY;
  case vfs::Status::NameTooLong: return STATUS_OBJECT_NAME_INVALID;
  case vfs::Status::NoSpace: return STATUS_DISK_FULL;
  case vfs::Status::ReadOnly: return STATUS_MEDIA_WRITE_PROTECTED;
  case vfs::Status::InvalidArgument:
  case vfs::Status::Range:
    return STATUS_INVALID_PARAMETER;
  case vfs::Status::NoData: return STATUS_NOT_SUPPORTED;
  case vfs::Status::IoError: return STATUS_IO_DEVICE_ERROR;
  }
  return STATUS_IO_DEVICE_ERROR;
}

bool wideToUtf8(PCWSTR input, std::string& output) {
  if (!input) return false;
  const int inputLength = static_cast<int>(wcslen(input));
  if (inputLength == 0) {
    output = "/";
    return true;
  }
  const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input,
                                            inputLength, nullptr, 0, nullptr, nullptr);
  if (required <= 0) return false;
  output.resize(static_cast<std::size_t>(required));
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, inputLength,
                          output.data(), required, nullptr, nullptr) != required) {
    output.clear();
    return false;
  }
  std::replace(output.begin(), output.end(), '\\', '/');
  if (output.front() != '/') output.insert(output.begin(), '/');
  return true;
}

bool utf8ToWide(const std::string& input, std::wstring& output) {
  if (input.empty()) {
    output.clear();
    return true;
  }
  if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
    return false;
  const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                            static_cast<int>(input.size()), nullptr, 0);
  if (required <= 0) return false;
  output.resize(static_cast<std::size_t>(required));
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                          static_cast<int>(input.size()), output.data(), required) != required) {
    output.clear();
    return false;
  }
  return true;
}

NTSTATUS backingPath(vfs::VFSCoreOperations* operations, const std::string& path,
                     std::wstring& output) {
  std::size_t pathSize = 0;
  NTSTATUS status = winfspStatus(
    operations->getExtendedAttribute(path, "vcmgr.path", nullptr, 0, pathSize));
  if (!NT_SUCCESS(status)) return status;
  std::string utf8(pathSize, '\0');
  status = winfspStatus(operations->getExtendedAttribute(
    path, "vcmgr.path", utf8.data(), utf8.size(), pathSize));
  if (!NT_SUCCESS(status)) return status;
  utf8.resize(pathSize);
  return utf8ToWide(utf8, output) ? STATUS_SUCCESS : STATUS_OBJECT_NAME_INVALID;
}

NTSTATUS readSecurityDescriptor(const std::wstring& path,
                                PSECURITY_DESCRIPTOR securityDescriptor,
                                SIZE_T* securityDescriptorSize) {
  if (!securityDescriptorSize) return STATUS_SUCCESS;
  DWORD required = 0;
  GetFileSecurityW(path.c_str(),
                   OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                     DACL_SECURITY_INFORMATION,
                   nullptr, 0, &required);
  const DWORD error = GetLastError();
  if (error != ERROR_INSUFFICIENT_BUFFER && error != ERROR_SUCCESS)
    return FspNtStatusFromWin32(error);
  if (!securityDescriptor || *securityDescriptorSize < required) {
    *securityDescriptorSize = required;
    return STATUS_BUFFER_OVERFLOW;
  }
  if (!GetFileSecurityW(path.c_str(),
                        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                          DACL_SECURITY_INFORMATION,
                        securityDescriptor, static_cast<DWORD>(*securityDescriptorSize),
                        &required))
    return FspNtStatusFromWin32(GetLastError());
  *securityDescriptorSize = required;
  return STATUS_SUCCESS;
}

std::uint64_t windowsTime(std::int64_t seconds) {
  const std::int64_t shifted = seconds + WindowsEpochOffset;
  if (shifted <= 0) return 0;
  return static_cast<std::uint64_t>(shifted) * WindowsTicksPerSecond;
}

std::int64_t unixTime(std::uint64_t ticks) {
  const std::uint64_t seconds = ticks / WindowsTicksPerSecond;
  if (seconds <= static_cast<std::uint64_t>(WindowsEpochOffset))
    return 0;
  return static_cast<std::int64_t>(seconds) - WindowsEpochOffset;
}

void makeFileInfo(const vfs::Metadata& metadata, FSP_FSCTL_FILE_INFO& info) {
  std::memset(&info, 0, sizeof(info));
  info.FileAttributes = metadata.isDirectory
    ? FILE_ATTRIBUTE_DIRECTORY
    : metadata.isReadOnly ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL;
  info.FileSize = metadata.size;
  info.AllocationSize = (metadata.size + 4095U) & ~std::uint64_t{4095U};
  info.CreationTime = windowsTime(metadata.changeTime);
  info.LastAccessTime = windowsTime(metadata.accessTime);
  info.LastWriteTime = windowsTime(metadata.modificationTime);
  info.ChangeTime = windowsTime(metadata.changeTime);
  info.HardLinks = static_cast<UINT32>(metadata.linkCount);
}

vfs::VFSCoreOperations* core(FSP_FILE_SYSTEM* filesystem) {
  return filesystem
    ? static_cast<vfs::VFSCoreOperations*>(filesystem->UserContext)
    : nullptr;
}

NTSTATUS metadataFor(vfs::VFSCoreOperations* operations, const std::string& path,
                     vfs::Metadata& metadata) {
  return operations ? winfspStatus(operations->getMetadata(path, metadata))
                    : STATUS_IO_DEVICE_ERROR;
}

bool wantsRead(UINT32 access) {
  return (access & (FILE_READ_DATA | FILE_EXECUTE | GENERIC_READ)) != 0;
}

bool wantsWrite(UINT32 access) {
  return (access & (FILE_WRITE_DATA | FILE_APPEND_DATA | GENERIC_WRITE)) != 0;
}

NTSTATUS checkAccess(UINT32 access, const vfs::Metadata& metadata) {
  if (wantsWrite(access) && metadata.isReadOnly) return STATUS_ACCESS_DENIED;
  return STATUS_SUCCESS;
}

NTSTATUS getVolumeInfo(FSP_FILE_SYSTEM* filesystem, FSP_FSCTL_VOLUME_INFO* info) {
  if (!filesystem || !info) return STATUS_INVALID_PARAMETER;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  vfs::StatFs stats;
  const auto status = operations->statfs("/", stats);
  if (status != vfs::Status::Ok) return winfspStatus(status);
  std::memset(info, 0, sizeof(*info));
  info->TotalSize = stats.blocks * stats.blockSize;
  info->FreeSize = stats.availableBlocks * stats.blockSize;
  return STATUS_SUCCESS;
}

NTSTATUS getSecurityByName(FSP_FILE_SYSTEM* filesystem, PWSTR fileName,
                           PUINT32 fileAttributes, PSECURITY_DESCRIPTOR securityDescriptor,
                           SIZE_T* securityDescriptorSize) {
  std::string path;
  if (!wideToUtf8(fileName, path)) return STATUS_OBJECT_NAME_INVALID;
  vfs::Metadata metadata;
  const NTSTATUS status = metadataFor(core(filesystem), path, metadata);
  if (!NT_SUCCESS(status)) return status;
  if (fileAttributes) {
    *fileAttributes = metadata.isDirectory
      ? FILE_ATTRIBUTE_DIRECTORY
      : metadata.isReadOnly ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL;
  }
  if (!securityDescriptorSize) return STATUS_SUCCESS;
  std::wstring physicalPath;
  // TODO: apparently non-existent file path can be returned
  const NTSTATUS pathStatus = backingPath(core(filesystem), path, physicalPath);
  return NT_SUCCESS(pathStatus)
    ? readSecurityDescriptor(physicalPath, securityDescriptor, securityDescriptorSize)
    : pathStatus;
}

NTSTATUS getFileInfo(FSP_FILE_SYSTEM* filesystem, PVOID fileContext,
                     FSP_FSCTL_FILE_INFO* fileInfo) {
  if (!fileContext || !fileInfo) return STATUS_INVALID_PARAMETER;
  const auto* context = static_cast<const FileContext*>(fileContext);
  vfs::Metadata metadata;
  const NTSTATUS status = metadataFor(core(filesystem), context->path, metadata);
  if (!NT_SUCCESS(status)) return status;
  makeFileInfo(metadata, *fileInfo);
  return STATUS_SUCCESS;
}

NTSTATUS open(FSP_FILE_SYSTEM* filesystem, PWSTR fileName, UINT32 createOptions,
              UINT32 grantedAccess, PVOID* fileContext,
              FSP_FSCTL_FILE_INFO* fileInfo) {
  if (!fileContext || !fileInfo) return STATUS_INVALID_PARAMETER;
  std::string path;
  if (!wideToUtf8(fileName, path)) return STATUS_OBJECT_NAME_INVALID;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;

  vfs::Metadata metadata;
  NTSTATUS status = metadataFor(operations, path, metadata);
  if (!NT_SUCCESS(status)) return status;
  const bool requestDirectory = (createOptions & FILE_DIRECTORY_FILE) != 0;
  const bool requestFile = (createOptions & FILE_NON_DIRECTORY_FILE) != 0;
  if (requestDirectory && !metadata.isDirectory) return STATUS_NOT_A_DIRECTORY;
  if (requestFile && metadata.isDirectory) return STATUS_FILE_IS_A_DIRECTORY;
  status = checkAccess(grantedAccess, metadata);
  if (!NT_SUCCESS(status)) return status;

  auto context = std::make_unique<FileContext>();
  context->path = path;
  context->isDirectory = metadata.isDirectory;
  if (!metadata.isDirectory) {
    vfs::OpenOptions options;
    options.read = wantsRead(grantedAccess);
    options.write = wantsWrite(grantedAccess);
    status = winfspStatus(operations->openFile(path, options, context->handle));
    if (!NT_SUCCESS(status)) return status;
  }
  makeFileInfo(metadata, *fileInfo);
  *fileContext = context.release();
  return STATUS_SUCCESS;
}

NTSTATUS create(FSP_FILE_SYSTEM* filesystem, PWSTR fileName, UINT32 createOptions,
                UINT32 grantedAccess, UINT32 fileAttributes, PSECURITY_DESCRIPTOR,
                UINT64 allocationSize, PVOID* fileContext,
                FSP_FSCTL_FILE_INFO* fileInfo) {
  if (!fileContext || !fileInfo) return STATUS_INVALID_PARAMETER;
  std::string path;
  if (!wideToUtf8(fileName, path)) return STATUS_OBJECT_NAME_INVALID;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  vfs::Metadata existing;
  NTSTATUS status = metadataFor(operations, path, existing);
  if (NT_SUCCESS(status)) return STATUS_OBJECT_NAME_COLLISION;
  if (status != STATUS_OBJECT_NAME_NOT_FOUND) return status;

  auto context = std::make_unique<FileContext>();
  context->path = path;
  context->isDirectory = (createOptions & FILE_DIRECTORY_FILE) != 0 ||
                         (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

  if (context->isDirectory) {
    status = winfspStatus(operations->createDirectory(path));
  } else {
    vfs::OpenOptions options;
    options.read = wantsRead(grantedAccess);
    options.write = true;
    options.create = true;
    status = winfspStatus(operations->createFile(path, options, context->handle));
  }
  if (!NT_SUCCESS(status)) return status;

  if (!context->isDirectory && allocationSize != 0) {
    status = winfspStatus(operations->setFileSize(path, allocationSize));
    if (!NT_SUCCESS(status)) {
      if (context->handle.id != 0) operations->close(context->handle.id);
      return status;
    }
  }
  vfs::Metadata metadata;
  status = metadataFor(operations, path, metadata);
  if (!NT_SUCCESS(status)) {
    if (context->handle.id != 0) operations->close(context->handle.id);
    return status;
  }
  if (fileAttributes & FILE_ATTRIBUTE_READONLY) metadata.isReadOnly = true;
  if (metadata.isReadOnly) {
    status = winfspStatus(operations->setAttributes(path, metadata));
    if (!NT_SUCCESS(status)) {
      if (context->handle.id != 0) operations->close(context->handle.id);
      return status;
    }
  }
  makeFileInfo(metadata, *fileInfo);
  *fileContext = context.release();
  return STATUS_SUCCESS;
}

NTSTATUS read(FSP_FILE_SYSTEM* filesystem, PVOID fileContext, PVOID buffer,
              UINT64 offset, ULONG length, PULONG bytesTransferred) {
  if (!fileContext || !bytesTransferred) return STATUS_INVALID_PARAMETER;
  auto* context = static_cast<FileContext*>(fileContext);
  if (context->isDirectory) return STATUS_FILE_IS_A_DIRECTORY;
  if (!core(filesystem)) return STATUS_IO_DEVICE_ERROR;
  std::size_t count = 0;
  auto output = std::span(static_cast<std::byte*>(buffer),
                          static_cast<std::size_t>(length));
  const NTSTATUS status = winfspStatus(
    core(filesystem)->read(context->handle.id, offset, output, count));
  if (!NT_SUCCESS(status)) return status;
  *bytesTransferred = static_cast<ULONG>(count);
  return STATUS_SUCCESS;
}

NTSTATUS write(FSP_FILE_SYSTEM* filesystem, PVOID fileContext, PVOID buffer,
               UINT64 offset, ULONG length, BOOLEAN writeToEndOfFile,
               BOOLEAN constrainedIo, PULONG bytesTransferred,
               FSP_FSCTL_FILE_INFO* fileInfo) {
  if (!fileContext || !bytesTransferred) return STATUS_INVALID_PARAMETER;
  auto* context = static_cast<FileContext*>(fileContext);
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  if (context->isDirectory) return STATUS_FILE_IS_A_DIRECTORY;

  vfs::Metadata metadata;
  NTSTATUS status = metadataFor(operations, context->path, metadata);
  if (!NT_SUCCESS(status)) return status;
  if (metadata.isReadOnly) return STATUS_ACCESS_DENIED;
  if (writeToEndOfFile) offset = metadata.size;
  if (constrainedIo) {
    if (offset >= metadata.size) {
      *bytesTransferred = 0;
      return STATUS_SUCCESS;
    }
    length = static_cast<ULONG>((std::min<std::uint64_t>)(
      length, metadata.size - offset));
  }
  std::size_t count = 0;
  auto input = std::span(static_cast<const std::byte*>(buffer),
                         static_cast<std::size_t>(length));
  status = winfspStatus(operations->write(context->handle.id, offset, input, count));
  if (!NT_SUCCESS(status)) return status;
  *bytesTransferred = static_cast<ULONG>(count);
  return fileInfo ? getFileInfo(filesystem, fileContext, fileInfo) : STATUS_SUCCESS;
}

VOID cleanup(FSP_FILE_SYSTEM* filesystem, PVOID fileContext, PWSTR fileName,
             ULONG flags) {
  if (!fileContext) return;
  auto* context = static_cast<FileContext*>(fileContext);
  if (context->cleaned) return;
  context->cleaned = true;
  if ((flags & FspCleanupDelete) == 0) return;

  auto* operations = core(filesystem);
  if (!operations) {
    qWarning() << "WinFsp cleanup could not reach VFS core";
    return;
  }
  if (!context->handle.id && fileName) {
    std::string callbackPath;
    if (wideToUtf8(fileName, callbackPath)) context->path = std::move(callbackPath);
  }
  if (context->handle.id != 0) {
    const auto pendingStatus = operations->setPendingDelete(context->handle.id, true);
    if (pendingStatus != vfs::Status::Ok) {
      qWarning() << "Failed to mark VFS handle for deletion";
      return;
    }
  }
  const vfs::Status deleteStatus = context->isDirectory
    ? operations->removeDirectory(context->path)
    : operations->unlink(context->path);
  if (deleteStatus != vfs::Status::Ok) {
    qWarning() << "WinFsp cleanup delete failed for" << QString::fromStdString(context->path)
               << Qt::hex << winfspStatus(deleteStatus);
  }
}

VOID close(FSP_FILE_SYSTEM* filesystem, PVOID fileContext) {
  std::unique_ptr<FileContext> context(static_cast<FileContext*>(fileContext));
  if (!context || context->handle.id == 0) return;
  auto* operations = core(filesystem);
  if (!operations) return;
  const vfs::Status status = operations->close(context->handle.id);
  if (status != vfs::Status::Ok)
    qWarning() << "Failed to close VFS file handle";
}

NTSTATUS canDelete(FSP_FILE_SYSTEM* filesystem, PVOID fileContext, PWSTR fileName) {
  std::string path;
  if (fileContext) path = static_cast<FileContext*>(fileContext)->path;
  else if (!wideToUtf8(fileName, path)) return STATUS_OBJECT_NAME_INVALID;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  vfs::Metadata metadata;
  NTSTATUS status = metadataFor(operations, path, metadata);
  if (!NT_SUCCESS(status)) return status;
  if (!metadata.isDirectory)
    return metadata.isReadOnly ? STATUS_ACCESS_DENIED : STATUS_SUCCESS;
  std::vector<vfs::DirectoryEntry> entries;
  status = winfspStatus(operations->readDirectory(path, entries));
  if (!NT_SUCCESS(status)) return status;
  return entries.size() > 2 ? STATUS_DIRECTORY_NOT_EMPTY : STATUS_SUCCESS;
}

NTSTATUS getSecurity(FSP_FILE_SYSTEM* filesystem, PVOID fileContext,
                     PSECURITY_DESCRIPTOR securityDescriptor,
                     SIZE_T* securityDescriptorSize) {
  if (!fileContext) return STATUS_INVALID_PARAMETER;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  auto* context = static_cast<FileContext*>(fileContext);
  std::wstring physicalPath;
  if (context->handle.resolvedPath.empty()) {
    const NTSTATUS status = backingPath(operations, context->path, physicalPath);
    if (!NT_SUCCESS(status)) return status;
  } else if (!utf8ToWide(context->handle.resolvedPath, physicalPath)) {
    return STATUS_OBJECT_NAME_INVALID;
  }
  return readSecurityDescriptor(physicalPath, securityDescriptor, securityDescriptorSize);
}

NTSTATUS rename(FSP_FILE_SYSTEM* filesystem, PVOID fileContext, PWSTR fileName,
                PWSTR newFileName, BOOLEAN replaceIfExists) {
  std::string oldPath;
  std::string newPath;
  if (fileContext) oldPath = static_cast<FileContext*>(fileContext)->path;
  else if (!wideToUtf8(fileName, oldPath)) return STATUS_OBJECT_NAME_INVALID;
  if (!wideToUtf8(newFileName, newPath)) return STATUS_OBJECT_NAME_INVALID;
  if (oldPath == newPath) return STATUS_SUCCESS;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;

  vfs::Metadata source;
  NTSTATUS status = metadataFor(operations, oldPath, source);
  if (!NT_SUCCESS(status)) return status;
  vfs::Metadata destination;
  status = metadataFor(operations, newPath, destination);
  if (NT_SUCCESS(status)) {
    if (!replaceIfExists) return STATUS_OBJECT_NAME_COLLISION;
    if (source.isDirectory != destination.isDirectory)
      return source.isDirectory ? STATUS_NOT_A_DIRECTORY : STATUS_FILE_IS_A_DIRECTORY;
    status = winfspStatus(destination.isDirectory
      ? operations->removeDirectory(newPath)
      : operations->unlink(newPath));
    if (!NT_SUCCESS(status)) return status;
  } else if (status != STATUS_OBJECT_NAME_NOT_FOUND) {
    return status;
  }
  status = winfspStatus(operations->rename(oldPath, newPath));
  if (NT_SUCCESS(status) && fileContext)
    static_cast<FileContext*>(fileContext)->path = std::move(newPath);
  return status;
}

NTSTATUS setBasicInfo(FSP_FILE_SYSTEM* filesystem, PVOID fileContext,
                      UINT32 fileAttributes, UINT64, UINT64 lastAccessTime,
                      UINT64 lastWriteTime, UINT64, FSP_FSCTL_FILE_INFO* fileInfo) {
  if (!fileContext) return STATUS_INVALID_PARAMETER;
  auto* context = static_cast<FileContext*>(fileContext);
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  vfs::Metadata metadata;
  NTSTATUS status = metadataFor(operations, context->path, metadata);
  if (!NT_SUCCESS(status)) return status;
  if (fileAttributes != INVALID_FILE_ATTRIBUTES)
    metadata.isReadOnly = (fileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
  if (lastAccessTime != 0) metadata.accessTime = unixTime(lastAccessTime);
  if (lastWriteTime != 0) metadata.modificationTime = unixTime(lastWriteTime);
  status = winfspStatus(operations->setAttributes(context->path, metadata));
  if (!NT_SUCCESS(status)) return status;
  return fileInfo ? getFileInfo(filesystem, fileContext, fileInfo) : STATUS_SUCCESS;
}

NTSTATUS setFileSize(FSP_FILE_SYSTEM* filesystem, PVOID fileContext,
                     UINT64 newSize, BOOLEAN setAllocationSize,
                     FSP_FSCTL_FILE_INFO* fileInfo) {
  if (!fileContext) return STATUS_INVALID_PARAMETER;
  auto* context = static_cast<FileContext*>(fileContext);
  if (context->isDirectory) return STATUS_FILE_IS_A_DIRECTORY;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  if (!setAllocationSize) {
    const NTSTATUS status = winfspStatus(
      operations->setFileSize(context->path, newSize));
    if (!NT_SUCCESS(status)) return status;
  }
  return fileInfo ? getFileInfo(filesystem, fileContext, fileInfo) : STATUS_SUCCESS;
}

NTSTATUS readDirectory(FSP_FILE_SYSTEM* filesystem, PVOID fileContext,
                       PWSTR, PWSTR marker, PVOID buffer, ULONG length,
                       PULONG bytesTransferred) {
  if (!fileContext || !bytesTransferred) return STATUS_INVALID_PARAMETER;
  auto* context = static_cast<FileContext*>(fileContext);
  if (!context->isDirectory) return STATUS_NOT_A_DIRECTORY;
  auto* operations = core(filesystem);
  if (!operations) return STATUS_IO_DEVICE_ERROR;
  std::vector<vfs::DirectoryEntry> entries;
  NTSTATUS status = winfspStatus(operations->readDirectory(context->path, entries));
  if (!NT_SUCCESS(status)) return status;
  *bytesTransferred = 0;

  std::wstring markerName;
  if (marker && *marker) markerName.assign(marker);
  bool afterMarker = markerName.empty();
  for (const auto& entry : entries) {
    std::wstring name;
    if (!utf8ToWide(entry.name, name)) return STATUS_OBJECT_NAME_INVALID;
    if (!afterMarker) {
      if (_wcsicmp(name.c_str(), markerName.c_str()) == 0) afterMarker = true;
      continue;
    }

    const std::size_t nameBytes = name.size() * sizeof(WCHAR);
    const std::size_t infoSize =
        FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + nameBytes;
    if (infoSize > (std::numeric_limits<UINT16>::max)())
      return STATUS_OBJECT_NAME_INVALID;

    alignas(FSP_FSCTL_DIR_INFO) std::vector<std::byte> storage(
        (std::max)(infoSize, sizeof(FSP_FSCTL_DIR_INFO)));
    auto* directoryInfo = reinterpret_cast<FSP_FSCTL_DIR_INFO*>(storage.data());
    directoryInfo->Size = static_cast<UINT16>(infoSize);
    vfs::Metadata metadata = entry.metadata;
    if (entry.name == "." || entry.name == "..") metadata.isDirectory = true;
    makeFileInfo(metadata, directoryInfo->FileInfo);
    if (nameBytes > 0) {
      std::memcpy(directoryInfo->FileNameBuf, name.data(), nameBytes);
    }
    if (!FspFileSystemAddDirInfo(directoryInfo, buffer, length, bytesTransferred))
      return *bytesTransferred == 0 ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
  }
  return STATUS_SUCCESS;
}

FSP_FILE_SYSTEM_INTERFACE fileSystemInterface = {
  .GetVolumeInfo = getVolumeInfo,
  .GetSecurityByName = getSecurityByName,
  .Create = create,
  .Open = open,
  .Cleanup = cleanup,
  .Close = close,
  .Read = read,
  .Write = write,
  .GetFileInfo = getFileInfo,
  .SetBasicInfo = setBasicInfo,
  .SetFileSize = setFileSize,
  .CanDelete = canDelete,
  .Rename = rename,
  .GetSecurity = getSecurity,
  .ReadDirectory = readDirectory,
};

} // namespace

class VFSInstance::Private {
public:
  Private(const QString& launcherRoot, const ModsIndex::Mod& mod)
      : mountpoint_(createMountPoint(QString::fromStdString(mod.id))),
        core_(vfs::createCore(launcherRoot.toStdString(), mod.id, mod.modPath)) {}

  ~Private() { unmount(); }

  bool mount() {
    std::lock_guard lock(mutex_);
    if (used_ || filesystem_) return false;
    used_ = true;

    FSP_FSCTL_VOLUME_PARAMS volumeParams{};
    volumeParams.SectorSize = 4096;
    volumeParams.SectorsPerAllocationUnit = 1;
    volumeParams.VolumeSerialNumber = GetTickCount();
    volumeParams.FileInfoTimeout = 1000;
    volumeParams.CaseSensitiveSearch = 0;
    volumeParams.CasePreservedNames = 1;
    volumeParams.UnicodeOnDisk = 1;
    volumeParams.PersistentAcls = 0;
    volumeParams.PassQueryDirectoryPattern = 0;
    wcscpy_s(volumeParams.FileSystemName,
             sizeof(volumeParams.FileSystemName) / sizeof(WCHAR), L"VirtualClub");

    NTSTATUS status = FspFileSystemCreate(L"" FSP_FSCTL_DISK_DEVICE_NAME,
                                          &volumeParams, &fileSystemInterface,
                                          &filesystem_);
    if (!NT_SUCCESS(status)) {
      qWarning() << "FspFileSystemCreate failed:" << Qt::hex << status;
      filesystem_ = nullptr;
      return false;
    }
    filesystem_->UserContext = core_.get();

    // nuke the mountpoint folder's existence if it exists
    // this is a requirement
    std::error_code error;
    std::filesystem::remove_all(mountpoint_.toStdWString(), error);
    if (error) {
      qWarning() << "Failed to auto-remove existing target mount folder:"
                 << QString::fromStdString(error.message());
      FspFileSystemDelete(filesystem_);
      filesystem_ = nullptr;
      return false;
    }
    
    auto mpWide = mountpoint_.toStdWString();
    status = FspFileSystemSetMountPoint(filesystem_, mpWide.data());
    if (!NT_SUCCESS(status)) {
      qWarning() << "FspFileSystemSetMountPoint failed:" << Qt::hex << status;
      FspFileSystemDelete(filesystem_);
      filesystem_ = nullptr;
      return false;
    }
    status = FspFileSystemStartDispatcher(filesystem_, 0);
    if (!NT_SUCCESS(status)) {
      qWarning() << "FspFileSystemStartDispatcher failed:" << Qt::hex << status;
      FspFileSystemDelete(filesystem_);
      filesystem_ = nullptr;
      return false;
    }
    return true;
  }

  void unmount() {
    std::lock_guard lock(mutex_);
    if (filesystem_) {
      FspFileSystemStopDispatcher(filesystem_);
      FspFileSystemDelete(filesystem_);
      filesystem_ = nullptr;
    }
    cleanupMountpoint();
  }

  QString mountPath() const { return mountpoint_; }

private:
  static QString createMountPoint(const QString& modId) {
    QTemporaryDir directory(QDir::tempPath() + "/vclubmgr_mount_" + modId + "_XXXXXX");
    if (!directory.isValid())
      throw std::runtime_error("Failed to create temporary WinFsp mount point");
    const QString path = QDir::toNativeSeparators(directory.path());
    directory.setAutoRemove(false);
    return path;
  }

  void cleanupMountpoint() {
    if (mountpoint_.isEmpty()) return;
    std::error_code error;
    std::filesystem::remove_all(mountpoint_.toStdWString(), error);
    if (error)
      qWarning() << "Failed to remove WinFsp mount point:"
                 << QString::fromStdString(error.message());
  }

  const QString mountpoint_;
  std::unique_ptr<vfs::VFSCoreOperations> core_;
  FSP_FILE_SYSTEM* filesystem_ = nullptr;
  bool used_ = false;
  std::mutex mutex_;
};

VFSInstance::VFSInstance(const QString& launcherRoot, const ModsIndex::Mod& mod)
    : d(std::make_unique<Private>(launcherRoot, mod)) {}

VFSInstance::~VFSInstance() = default;
bool VFSInstance::mount() { return d->mount(); }
void VFSInstance::unmount() { d->unmount(); }
QString VFSInstance::mountPath() const { return d->mountPath(); }

#else

VFSInstance::VFSInstance(const QString&, const ModsIndex::Mod&) {
  throw std::runtime_error("VFS is not available on this platform.");
}
VFSInstance::~VFSInstance() = default;
bool VFSInstance::mount() { return false; }
void VFSInstance::unmount() {}
QString VFSInstance::mountPath() const { return {}; }

#endif
