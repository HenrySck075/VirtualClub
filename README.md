looks smth like this atm

![](readme_assets/ss1.png)

(dont mind the title bar pls thx)

## How Does This Work
long answer, magic

short answer, the "virtual filesystem" technology allows the launcher to construct a fake folder that replicates the traditional setup of a Ren'Py mod installation without actually consuming more disk spaces from making a copy of both the base game and the mod.<br>meaning you get the benefit of disk space efficiency AND isolated save content (as the save dir was modified to name after its id) from the purpose-made Doki Doki Mod Docker while being a separate app. (and qt/c++ launches faster than renpy/python anyway)


## Dependencies
`qt6`, `pybind11`, `fuse3` (winfsp on windows)
