init -2026 python early:
    def setDeveloperMode():
        import os
        if os.getenv("MVC_DEVELOPER", None) is not None:
            config.developer = True

    setDeveloperMode()

init python:
    setDeveloperMode()

# we're just forcing atp
init 2 python:
    setDeveloperMode()

init 999 python:
    setDeveloperMode()


# initialize VERY LATE to make sure it wont be overridden by the original
init 999 python:
    import os
    import sys

    def get_qt_app_data_location(org_name, app_name):
        """
        Replicates QStandardPaths.writableLocation(QStandardPaths.AppDataLocation)
        Compatible with Python 2 and Python 3.
        """
        # Detect platform
        platform = sys.platform

        if platform.startswith("win"):
            # Windows: APPDATA (Roaming) folder
            # Safe environment lookup fallback to user profile if APPDATA is missing
            base_dir = os.environ.get("APPDATA")
            if not base_dir:
                base_dir = os.path.join(os.environ.get("USERPROFILE", "C:\\"), "AppData", "Roaming")
            
            # Qt formats AppDataLocation as AppData/Roaming/OrgName/AppName on Windows
            return os.path.join(base_dir, org_name, app_name)

        elif platform == "darwin":
            # macOS: ~/Library/Application Support/OrgName/AppName
            home = os.path.expanduser("~")
            return os.path.join(home, "Library", "Application Support", org_name, app_name)

        else:
            # Linux / Unix: Uses XDG_DATA_HOME or defaults to ~/.local/share
            # Qt format: ~/.local/share/OrgName/AppName
            base_dir = os.environ.get("XDG_DATA_HOME")
            if not base_dir:
                base_dir = os.path.join(os.path.expanduser("~"), ".local", "share")
            
            return os.path.join(base_dir, org_name, app_name)

# 1. Calculate the exact standard AppData location
    launcherDataLocation = get_qt_app_data_location("MetaverseEnterprise", "VirtualClub")

# 2. balls

    mod_uuid = os.getenv("MVC_MOD_ID") # variable provided by the launcher

    icon_filename = mod_uuid + ".scaled.png"
    absolute_icon_path = os.path.join(launcherDataLocation, "icons", icon_filename)

# 3. Apply the icon path directly to Ren'Py's configuration
# Ren'Py accepts absolute path strings for config.window_icon if initialized early
    config.window_icon = None

    try:
        try:
            import pygame_sdl2 as pygame
        except ImportError:
            import pygame
        # Load the image using Pygame's native loader (bypasses Ren'Py's file system)
        native_surface = pygame.image.load(absolute_icon_path)
        
        # Direct SDL window update
        pygame.display.set_icon(native_surface)
        print("Launcher icon set to: {}".format(absolute_icon_path))
    except Exception as e:
        # Fallback or log if the image is corrupted or missing
        print("Failed to apply custom window icon: {}".format(e))


init -67 python early:
    mod_uuid = os.getenv("MVC_MOD_ID") # variable provided by the launcher
    config.save_directory = mod_uuid


init 999 python:
    def _autoload_check():
        import os
        maybeSaveID = os.environ.pop("MVC_SAVE_ID", None)
        if maybeSaveID:
            import renpy
            renpy.loadsave.load(maybeSaveID)
        config.periodic_callbacks.remove(_autoload_check)

    # conveniently start_callbacks exists since v6.99.11 so base ddlc will still let this through
    config.periodic_callbacks.append(_autoload_check)
