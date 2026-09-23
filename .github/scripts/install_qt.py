import os
import sys
import shutil
import urllib.request
import xml.etree.ElementTree as ET
import subprocess

# Define additional modules to fetch alongside core Qt (e.g., ["qtwebsockets", "qtsvg"])
MODULES = [
    # "qtwebsockets",
    # "qtsvg",
    "qtmultimedia",
]

USER_AGENT = "Mozilla/5.0 (Linux; Android 11; SM-G970F) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.105 Mobile Safari/537.36"

def install_qt():
    if sys.platform == "win32":
        base_url = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6112/qt6_6112_msvc2022_64/"
        arch = "win64_msvc2022_64"
    else:
        base_url = "https://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt6_6112/qt6_6112/"
        arch = "linux_gcc_64"

    updates_url = base_url + "Updates.xml"
    target_dir = os.path.abspath("qt6")
    os.makedirs(target_dir, exist_ok=True)

    print(f"Fetching metadata from {updates_url}...")
    req = urllib.request.Request(updates_url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req) as resp:
        xml_content = resp.read()

    root = ET.fromstring(xml_content)
    archives: dict[str, list[str]] = {}
    archives_count = 0
    matched_packages = []

    for pkg in root.findall(".//PackageUpdate"):
        name_elem = pkg.find("Name")
        if name_elem is None or not name_elem.text:
            continue

        pkg_name = name_elem.text.strip()

        # 1. Skip debug information, source archives, documentation, and examples
        if any(skip in pkg_name for skip in ["debug", "doc", "examples", "src"]):
            continue

        # 2. Ensure package matches target platform architecture
        if not pkg_name.endswith(arch):
            continue

        # 3. Determine if package is Core (qt.qt6.6112.<arch>) or a requested Module
        clean_name = pkg_name[:-len(arch)].rstrip(".")
        parts = clean_name.split(".")
        
        # Core packages have base structure (e.g., ['qt', 'qt6', '6112'])
        is_core = len(parts) <= 3
        module_name = parts[-1] if not is_core else None

        if is_core or (module_name in MODULES):
            archives_elem = pkg.find("DownloadableArchives")
            module_version_elem = pkg.find("Version")
            module_version = ""
            if module_version_elem is not None and module_version_elem.text:
                module_version = module_version_elem.text.strip()
            if archives_elem is not None and archives_elem.text:
                matched_packages.append(pkg_name)
                if pkg_name not in archives:
                    archives[pkg_name] = []
                for item in archives_elem.text.split(","):
                    item = item.strip()
                    if item and item not in archives[pkg_name]:
                        archives[pkg_name].append(module_version+item)
                        archives_count += 1

    print(f"Matched packages: {matched_packages}")
    print(f"Found {archives_count} archive(s) to download.")

    seven_zip = shutil.which("7z") or shutil.which("7za") or shutil.which("7zz")
    if not seven_zip:
        raise RuntimeError("7-Zip utility (7z/7za) not found in PATH.")

    temp_dir = os.path.abspath("qt_temp")
    os.makedirs(temp_dir, exist_ok=True)

    for package, pkg_archives in archives.items():
        for archive_name in pkg_archives:
            archive_url = base_url + package + "/" + archive_name
            archive_file = os.path.join(temp_dir, archive_name)
            
            print(f"Downloading {archive_url}...")
            # dont ask me idk why
            req = urllib.request.Request(archive_url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req) as resp, open(archive_file, "wb") as out_file:
                shutil.copyfileobj(resp, out_file)

            print(f"Extracting {archive_name}...")
            subprocess.run([seven_zip, "x", archive_file, f"-o{target_dir}", "-y"], check=True)
            os.remove(archive_file)

    shutil.rmtree(temp_dir, ignore_errors=True)

    # Configure relocatable qt.conf
    bin_dir = os.path.join(target_dir, "bin")
    os.makedirs(bin_dir, exist_ok=True)
    with open(os.path.join(bin_dir, "qt.conf"), "w", encoding="utf-8") as f:
        f.write("[Paths]\nPrefix = ..\n")


    # Export paths to GITHUB_ENV and GITHUB_PATH
    github_env = os.environ.get("GITHUB_ENV")
    if github_env:
        lib_dir = os.path.join(target_dir, "lib")
        with open(github_env, "a", encoding="utf-8") as f:
            f.write(f"CMAKE_PREFIX_PATH={target_dir}\n")
            f.write(f"Qt6_DIR={os.path.join(target_dir, 'lib', 'cmake', 'Qt6')}\n")
            # Ensure Linux dynamically links against Qt's extracted ICU libraries:
            if sys.platform != "win32":
                current_ld = os.environ.get("LD_LIBRARY_PATH", "")
                f.write(f"LD_LIBRARY_PATH={target_dir}:{current_ld}\n")

    github_path = os.environ.get("GITHUB_PATH")
    if github_path:
        with open(github_path, "a", encoding="utf-8") as f:
            f.write(f"{bin_dir}\n")

    print(f"Qt 6.11.2 successfully setup at {target_dir}")

if __name__ == "__main__":
    install_qt()
