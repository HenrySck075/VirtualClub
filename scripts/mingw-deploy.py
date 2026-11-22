#!/usr/bin/env python3
import argparse
import re
import shutil
import subprocess
from collections import deque
from pathlib import Path

IMPORT_RE = re.compile(r"^\s*DLL Name:\s*(\S+\.dll)\s*$", re.IGNORECASE)

QT_PLUGIN_ITEMS = [
    "platforms/qwindows.dll",
    "styles",
    "imageformats",
    "iconengines",
    "multimedia",
    "tls",
    "networkinformation",
]


def is_qt_dll(name: str) -> bool:
    low = name.lower()
    return low.startswith("qt6") and low.endswith(".dll")


def get_pe_imports(objdump: str, binary: Path) -> list[str]:
    res = subprocess.run(
        [objdump, "-p", str(binary)],
        capture_output=True,
        text=True,
        check=True,
    )
    return [m.group(1) for line in res.stdout.splitlines() if (m := IMPORT_RE.match(line))]


def find_qt_plugins_dir(mingw_prefix: Path) -> Path:
    for candidate in (
        mingw_prefix / "share" / "qt6" / "plugins",
        mingw_prefix / "lib" / "qt6" / "plugins",
        mingw_prefix / "plugins",
    ):
        if candidate.is_dir():
            return candidate
    raise FileNotFoundError(f"Could not locate Qt6 plugins directory under {mingw_prefix}")


def copy_if_newer(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if (
        not dst.exists()
        or src.stat().st_mtime > dst.stat().st_mtime
        or src.stat().st_size != dst.stat().st_size
    ):
        shutil.copy2(src, dst)


def main() -> None:
    parser = argparse.ArgumentParser(description="Copy Qt6 DLLs and plugins for mingw-w64 builds")
    parser.add_argument("--target", action="append", required=True, help="Built .exe to scan for Qt6 DLLs")
    parser.add_argument("--dist-dir", required=True, help="Destination package directory")
    parser.add_argument("--mingw-prefix", default="/usr/x86_64-w64-mingw32", help="MinGW sysroot prefix")
    parser.add_argument("--objdump", default="x86_64-w64-mingw32-objdump", help="MinGW objdump binary")
    args = parser.parse_args()

    dist_dir = Path(args.dist_dir).resolve()
    mingw_prefix = Path(args.mingw_prefix).resolve()
    mingw_bin = mingw_prefix / "bin"
    qt_plugins_root = find_qt_plugins_dir(mingw_prefix)

    queue: deque[Path] = deque(Path(t).resolve() for t in args.target)
    visited: set[Path] = set()

    # Stage Qt6 plugins first and queue them so any Qt6 DLLs they import are copied too
    for rel in QT_PLUGIN_ITEMS:
        src = qt_plugins_root / rel
        if src.is_file():
            dst = dist_dir / "plugins" / rel
            copy_if_newer(src, dst)
            queue.append(dst)
        elif src.is_dir():
            for dll in src.rglob("*.dll"):
                dst = dist_dir / "plugins" / dll.relative_to(qt_plugins_root)
                copy_if_newer(dll, dst)
                queue.append(dst)

    # Walk only Qt6*.dll imports recursively
    while queue:
        binary = queue.popleft()
        if binary in visited or not binary.exists():
            continue
        visited.add(binary)

        for dll_name in get_pe_imports(args.objdump, binary):
            if not is_qt_dll(dll_name):
                continue
            src_dll = mingw_bin / dll_name
            if not src_dll.is_file():
                # Fallback case-insensitive lookup in mingw_bin
                match = next((p for p in mingw_bin.glob("Qt6*.dll") if p.name.lower() == dll_name.lower()), None)
                if match is None:
                    continue
                src_dll = match
            dst_dll = dist_dir / src_dll.name
            copy_if_newer(src_dll, dst_dll)
            queue.append(dst_dll)

    (dist_dir / "qt.conf").write_text("[Paths]\nPlugins = .\n", encoding="utf-8")


if __name__ == "__main__":
    main()
