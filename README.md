# VirtualClub: A(nother) DDLC Mod Loader

A brand new way to "install" and get a **completely isolated** mod session in just a few clicks, without ever worrying about if you installed the mod right!

wait whats with the "install" in quotation mark? well theres **no files copying happening behind the scene at all**! using [pure magic](libbivfs/src/module.cpp), you can play any* mods without wasting any more disk space from a second unnecessary copy. 

> TODO: allow relocating mods folder

See the usage demo video [here](https://youtu.be/1gYvaCy5jng)

*This project is unaffiliated with Team Salvato, just in case it wasn't THAT obvious. in all honesty i think this works with any renpy games but the main focus is ddlc so yeah.*

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
