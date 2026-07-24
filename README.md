# VirtualClub: A(nother) DDLC Mod Loader

A brand new way to "install" and get a **completely isolated** mod session in just a few clicks, without ever worrying about if you installed the mod right!

wait whats with the "install" in quotation mark? well theres **no files copying happening behind the scene at all**! using [pure magic](libbivfs/src/module.cpp), you can play any* mods without wasting any more disk space from a second unnecessary copy. You just need to keep one copy of the extracted mod folder though.

See the usage demo video [here](https://youtu.be/1gYvaCy5jng), and download the program from the Releases section at the right side of your page.

*This project is unaffiliated with Team Salvato, just in case it wasn't THAT obvious. in all honesty i think this works with any renpy games using engine version >= 6.99.11 but the main focus is ddlc so yeah.*

## Usage
0. On the main configuration screen, click the "Change folder" button and select the extracted original game's folder (the one containing the .exe and allat)
1. Click the plus button in the bottom left corner to add a new mod, then do as instructed. <br/>In a nutshell, a folder with *valid Ren'Py game structure* should have the `game` subfolder at the minimum and otherwise mirrors the structure of a normal Ren'Py game's folder. It didn't validate atm, but it will backstab you if you did it wrong.<br/>You must ***NOT*** delete this folder.
2. That's it! A screen for your newly installed mod should appear in an instant and accompanied by a new item in the left navigation bar. Click the "Start" button and you're in!

## Other usages (ig)
- Click the "Edit" button in the mod's screen and it will allow you to change the mod's name, version, icon and the folder path if you have to move the mod's folder somewhere else. The name and version only applies to the app and won't affect the game itself, though the icon does because of Date to Dream Of.
- Wanted to start from a save directly? Click the "Start from save" button tucked away inside the arrow next to the start button, select a save, and you're back where you left off.
- !!developer mode toggle!! Aside from enabling the engine's developer features (duh), it also come with sprite previewer (Shift-D) to browser all Composite sprites. *very* laggy if contains bunch of sprites though.
- You can also uninstall a mod, though it just means removing the entry and nothing else.

## Development
Clone the repo:
```
git clone https://github.com/HenrySck075/VirtualClub
```

### Python
Install uv using your Linux distro's package manager or [through standalone method](https://docs.astral.sh/uv/getting-started/installation/), then run:
```
uv sync
```

### C++ (libbivfs)
Install cmake and the following dependencies:
- pybind11
- libfuse3/WinFSP

Configure the library and build:
```
cmake -B libbivfs/build -S libbivfs
cmake --build libbivfs/build
```

Symlink or move the built library to the root folder so Python can find the library. (yeah)


## actually running the app
```
uv run main.py
```
