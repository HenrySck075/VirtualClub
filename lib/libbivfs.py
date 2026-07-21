import os,errno
import os
import errno
from typing import Any, Dict, List
from fuse import Operations, FuseOSError

class LibbiVFS(Operations):
    """Slightly more convoluted union vfs but it gets the job done"""
    def __init__(
        self, 
        baseFolder: str, 
        modFolder: str, 
        isRenpyGameFolder: bool
    ):
        self.baseFolder = baseFolder
        self.modFolder = modFolder
        self.isRenpyGameFolder = isRenpyGameFolder

    # --- Core VFS Read/Write Operations ---

    def getPath(self, path: str, write: bool = False) -> str:
        # for lib/**/* and renpy/**/*, ONLY look inside the mod folder 
        # (if those top-level folders exist due to different engine versions, otherwise its not)
        # for every other folders, do the normal delta path lookup
        # write always point to the mod folder. 

        toModFolder = os.path.join(self.modFolder, path.lstrip('/'))
        toBaseFolder = os.path.join(self.baseFolder, path.lstrip('/'))

        if path.startswith('/lib') or path.startswith('/renpy'):
            ret = toModFolder
            if self.isRenpyGameFolder or not os.path.exists(ret):
                ret = toBaseFolder
            return ret
        else:
            if write:
                # this might not be a good idea. probably should place them under a subfolder or sum
                if self.isRenpyGameFolder: 
                    if path.startswith('/game'): 
                        return os.path.join(self.modFolder, path.lstrip('/game/'))
                    else: 
                        return toBaseFolder
                else:
                    return toModFolder
            else:
                ret = toModFolder
                if not os.path.exists(ret):
                    ret = toBaseFolder
                return ret

    # --- Read Operations ---

    def getattr(self, path: str, fh: Any = None) -> Dict[str, Any]:
        full_path = self.getPath(path)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)

        st = os.lstat(full_path)
        return {
            key: getattr(st, key) 
            for key in (
                'st_atime', 'st_ctime', 'st_gid', 'st_mode', 
                'st_mtime', 'st_nlink', 'st_size', 'st_uid'
            )
        }

    def readdir(self, path: str, fh: Any) -> List[str]:
        dirents = {'.', '..'}
        
        # In a Union VFS, we need to merge listings from both mod and base folders
        mod_path = self.getPath(path, write=False) # Resolves to modFolder first
        base_path = os.path.join(self.baseFolder, path.lstrip('/'))

        found = False
        if os.path.isdir(mod_path):
            found = True
            dirents.update(os.listdir(mod_path))
            
        if os.path.isdir(base_path):
            found = True
            dirents.update(os.listdir(base_path))

        if not found:
            raise FuseOSError(errno.ENOENT)

        return list(dirents)

    def read(self, path: str, size: int, offset: int, fh: Any) -> bytes:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)

        try:
            with open(full_path, 'rb') as f:
                f.seek(offset)
                return f.read(size)
        except Exception:
            raise FuseOSError(errno.EIO)

    def readlink(self, path: str) -> str:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        return os.readlink(full_path)

    def access(self, path: str, amode: int) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        if not os.access(full_path, amode):
            raise FuseOSError(errno.EACCES)
        return 0

    def statfs(self, path: str) -> Dict[str, Any]:
        full_path = self.getPath(path)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        stv = os.statvfs(full_path)
        return {
            key: getattr(stv, key)
            for key in (
                'f_bavail', 'f_bfree', 'f_blocks', 'f_bsize', 'f_favail',
                'f_ffree', 'f_files', 'f_flag', 'f_frsize', 'f_namemax'
            )
        }

    # --- Write Operations ---

    def create(self, path: str, mode: int, fi: Any = None) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if os.path.exists(full_path):
            raise FuseOSError(errno.EEXIST)

        # Ensure parent directories exist in target write directory
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        
        try:
            fd = os.open(full_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, mode)
            os.close(fd)
        except Exception:
            raise FuseOSError(errno.EIO)
        return 0

    def write(self, path: str, data: bytes, offset: int, fh: Any) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)

        try:
            with open(full_path, 'r+b') as f:
                f.seek(offset)
                f.write(data)
            return len(data)
        except Exception:
            raise FuseOSError(errno.EIO)

    def truncate(self, path: str, length: int, fh: Any = None) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        # If truncating a file that only exists in base, copy it to write path first
        if not os.path.exists(full_path):
            base_path = self.getPath(path, write=False)
            if os.path.exists(base_path):
                os.makedirs(os.path.dirname(full_path), exist_ok=True)
                with open(base_path, 'rb') as f_src, open(full_path, 'wb') as f_dst:
                    f_dst.write(f_src.read())
            else:
                raise FuseOSError(errno.ENOENT)

        with open(full_path, 'r+b') as f:
            f.truncate(length)
        return 0

    def mkdir(self, path: str, mode: int) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if os.path.exists(full_path):
            raise FuseOSError(errno.EEXIST)
        os.makedirs(full_path, mode)
        return 0

    def rmdir(self, path: str) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        os.rmdir(full_path)
        return 0

    def unlink(self, path: str) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        os.unlink(full_path)
        return 0

    def rename(self, old: str, new: str) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        old_full = self.getPath(old, write=True)
        new_full = self.getPath(new, write=True)
        
        # If old only exists in base, write-copy it before renaming
        if not os.path.exists(old_full):
            old_base = self.getPath(old, write=False)
            if os.path.exists(old_base):
                os.makedirs(os.path.dirname(old_full), exist_ok=True)
                with open(old_base, 'rb') as src, open(old_full, 'wb') as dst:
                    dst.write(src.read())
            else:
                raise FuseOSError(errno.ENOENT)

        os.makedirs(os.path.dirname(new_full), exist_ok=True)
        os.rename(old_full, new_full)
        return 0

    def chmod(self, path: str, mode: int) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        os.chmod(full_path, mode)
        return 0

    def utimens(self, path: str, times: Any = None) -> int:  # type: ignore[reportIncompatibleMethodOverride]
        full_path = self.getPath(path, write=True)
        if not os.path.exists(full_path):
            raise FuseOSError(errno.ENOENT)
        os.utime(full_path, times)
        return 0
