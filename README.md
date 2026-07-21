# Development
Clone the repo:
```
git clone https://github.com/HenrySck075/VirtualClub
```

## Python
Install uv using your Linux distro's package manager or [through standalone method](https://docs.astral.sh/uv/getting-started/installation/), then run:
```
uv sync
```

## C++ (libbivfs)
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
