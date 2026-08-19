init -9999 python:
    import store
    import os

    class AlwaysTrue:
        def __get__(self, obj, objtype=None):
            return True
        def __set__(self, obj, value):
            pass  # Ignore any attempts by the game to set it to False

    if os.getenv("MVC_DEVELOPER", None) is not None:
        type(store.config).developer = AlwaysTrue()


# initialize VERY LATE to make sure it wont be overridden by the original
init 999 python:
    import os
    import sys

    def get_app_data_location(app_name):
        # Detect platform
        platform = sys.platform

        if platform.startswith("win"):
            # Windows: APPDATA (Roaming) folder
            # Safe environment lookup fallback to user profile if APPDATA is missing
            base_dir = os.environ.get("APPDATA")
            if not base_dir:
                base_dir = os.path.join(os.environ.get("USERPROFILE", "C:\\"), "AppData", "Roaming")
            
            # Qt formats AppDataLocation as AppData/Roaming/AppName on Windows
            return os.path.join(base_dir, app_name)

        elif platform == "darwin":
            # macOS: ~/Library/Application Support/AppName
            home = os.path.expanduser("~")
            return os.path.join(home, "Library", "Application Support", app_name)

        else:
            # Linux / Unix: Uses XDG_DATA_HOME or defaults to ~/.config
            # Qt format: ~/.config/AppName
            base_dir = os.environ.get("XDG_DATA_HOME")
            if not base_dir:
                base_dir = os.path.join(os.path.expanduser("~"), ".config")
            
            return os.path.join(base_dir, app_name)

# 1. Calculate the exact standard AppData location
    launcherDataLocation = get_app_data_location("VirtualClub")

# 2. balls

    mod_uuid = os.getenv("MVC_MOD_ID") # variable provided by the launcher

    icon_filename = mod_uuid + ".icon.png"
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
    skipper = 0
    import os
    maybeSaveID = os.environ.pop("MVC_SAVE_ID", None)
    if maybeSaveID:
        def _autoload_check():
            if skipper > 3:
                import renpy
                if hasattr(maybeSaveID, "removesuffix"):
                    maybeSaveID = maybeSaveID.removesuffix(renpy.savegame_suffix)
                else:
                    # For Python versions < 3.9, use rstrip instead
                    maybeSaveID = maybeSaveID.rstrip(renpy.savegame_suffix)
                raise Exception(f"Autoloading save: {maybeSaveID}") # literally the only way to see the logs
                renpy.loadsave.load(maybeSaveID)
                config.periodic_callbacks.remove(_autoload_check)
            skipper+=1

        config.periodic_callbacks.append(_autoload_check)
