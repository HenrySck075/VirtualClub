init 999 python:
    import os
    # Ren'Py 6 transitioned from Pygame to Pygame_SDL2, so we try both to be safe.
    try:
        import pygame_sdl2 as pygame
    except ImportError:
        import pygame

    # Global variables for the screen state
    search_query = ""
    all_composites = []

    # init 999 ensures this runs AFTER all your 'image' statements have been registered
    for name_tuple, disp in renpy.display.image.images.items():
        # Check specifically for im.Composite
        if isinstance(disp, renpy.display.im.Composite):
            name_str = " ".join(name_tuple)
            all_composites.append((name_str, disp))

    # Sort alphabetically by image name for easier browsing
    all_composites.sort(key=lambda x: x[0])

    def save_composite(disp, filename):
        try:
            # .load() forces the manipulator to render and returns a surface
            surf = disp.load()
            
            # Save it to the root of your project directory (config.basedir)
            # This avoids permissions issues on Arch/Linux compared to game/ directory
            safe_filename = filename.replace(" ", "_") + ".png"
            filepath = os.path.join(config.basedir, safe_filename)
            
            pygame.image.save(surf, filepath)
            renpy.notify("Saved to: " + filepath)
        except Exception as e:
            renpy.notify("Failed to save: " + str(e))

screen composite_previewer():
    # Dynamically filter the list based on the search query during each interaction
    $ current_list = [c for c in all_composites if search_query.lower() in c[0].lower()]
    
    default current_disp = None
    default current_name = ""

    frame:
        xalign 0.5 yalign 0.5
        xysize (1100, 650)
        
        hbox:
            spacing 20
            
            # Left Column: Search & List
            vbox:
                xsize 300
                spacing 10
                
                frame:
                    xfill True
                    vbox:
                        text "Search Images:" size 20
                        # VariableInputValue triggers a screen update automatically when typed into
                        input value VariableInputValue("search_query") length 30
                        
                        textbutton "Clear" action SetVariable("search_query", "") xalign 1.0

                viewport:
                    scrollbars "vertical"
                    mousewheel True
                    xfill True yfill True
                    
                    vbox:
                        spacing 2
                        if not current_list:
                            text "No composites found." size 16
                        else:
                            for name, disp in current_list:
                                textbutton name:
                                    action [SetScreenVariable("current_disp", disp), SetScreenVariable("current_name", name)]
                                    text_size 16

            # Right Column: Preview & Download
            vbox:
                xsize 760
                spacing 15
                
                if current_disp:
                    hbox:
                        xfill True
                        text current_name size 28 bold True xalign 0.0
                        textbutton "Download PNG" action Function(save_composite, current_disp, current_name) xalign 1.0
                    
                    # Preview Frame
                    frame:
                        xfill True yfill True
                        background Solid("#222222") # Dark background to see the edges of transparent images
                        
                        # Add the displayable directly to the screen
                        add current_disp align (0.5, 0.5)
                        
                else:
                    text "Select an image from the list to preview." align (0.5, 0.5) size 24

    # Add a close button
    textbutton "Close" action Hide("composite_previewer") align (1.0, 0.0) offset (-10, 10)
    
init python:
    if config.developer:
        # 1. Define a brand new keybind event name and bind a key to it (e.g., Shift + D)
        config.keymap['trigger_dev_mode'] = [ 'shift_K_d' ]

        # 2. Tell Ren'Py what to do when that event happens
        # Under the hood, we check for our event during game interactions
        def dev_key_checker():
            if renpy.get_screen("composite_previewer"):
                renpy.hide_screen("composite_previewer")
            else:
                renpy.show_screen("composite_previewer")
            
            # Refresh the screen so the change happens instantly
            renpy.restart_interaction() 

        # 3. Bind your Python function to the key event
        # (Note: In older v6 builds, assigning directly via config underlays was common)
        config.underlay.append(renpy.Keymap(trigger_dev_mode=dev_key_checker))
