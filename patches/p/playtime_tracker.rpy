init python:
    import os
    import time

    class PlaytimeTracker(object):
        def __init__(self):
            self.raw_time = 0.0
            self.active_time = 0.0
            self.last_update = time.time()
            self.last_activity = time.time()
            self.last_save = 0.0
            self.write_delay = 4.0
            self.loaded = False

        def load(self):
            """Loads existing playtime from .mvc_playtime if present."""
            if self.loaded:
                return
            self.loaded = True

            if not config.savedir:
                return

            filepath = os.path.join(config.savedir, ".mvc_playtime")
            if os.path.exists(filepath):
                try:
                    with open(filepath, "r") as f:
                        data = f.read().strip().split()
                        if len(data) >= 2:
                            self.raw_time = float(data[0])
                            self.active_time = float(data[1])
                        elif len(data) == 1:
                            # Fallback if only one value exists from prior setup
                            self.raw_time = float(data[0])
                            self.active_time = float(data[0])
                except Exception:
                    pass

        def on_interact(self):
            """Called whenever a new interaction begins (dialogue advance, choice, etc.)."""
            self.last_activity = time.time()

        def is_menu_present(self):
            """Checks if the game is currently in the main menu, pause menu, or choice screen."""
            #if renpy.context().main_menu:
            #    return True

            menu_screens = ("choice", "navigation", "game_menu", "pause", "save", "load", "preferences", "history", "about")
            for screen in menu_screens:
                if renpy.get_screen(screen):
                    return True
            return False

        def update(self):
            """Main updates loop executed periodically by Ren'Py."""
            
            if not config.savedir:
                return

            self.load()

            now = time.time()
            dt = now - self.last_update
            self.last_update = now

            # Protect against computer sleep/wake or massive time jumps
            if dt < 0 or dt > 10.0:
                dt = self.write_delay

            # 1. Raw clock (always advances)
            self.raw_time += dt

            # 2. Active clock (subject to AFK thresholds)
            idle_time = now - self.last_activity
            threshold = 120.0 if self.is_menu_present() else 30.0

            prev_idle = idle_time - dt
            if prev_idle < threshold:
                # Add only the fraction of dt that occurred before reaching the threshold
                effective_dt = min(dt, threshold - prev_idle)
                if effective_dt > 0:
                    self.active_time += effective_dt

            # Throttled write to disk (every 1 second)
            if now - self.last_save >= self.write_delay:
                self.save()
                self.last_save = now

        def save(self):
            print(not not config.savedir)
            """Writes current playtime values to .mvc_playtime."""
            if not config.savedir:
                return

            filepath = os.path.join(config.savedir, ".mvc_playtime")
            try:
                try:
                    os.makedirs(config.savedir, exist_ok=True)
                except TypeError:
                    try: # python 2 does not have exist_ok. im too lazy to import the PY2 constant okay?
                        os.makedirs(config.savedir)
                    except: pass
                except: pass
                
                with open(filepath, "w") as f:
                    f.write("{:.0f} {:.0f}".format(self.raw_time, self.active_time))
            except Exception as e:
                print(e)

    # Instantiate tracker
    playtime_tracker = PlaytimeTracker()

    # Hook into Ren'Py callbacks
    config.periodic_callbacks.append(playtime_tracker.update)
    config.start_interact_callbacks.append(playtime_tracker.on_interact)
