> This section is mainly for developers. Regular users should skip straight to [the Releases page](https://github.com/HenrySck075/VirtualClub/releases).


looks smth like this atm

![The nothing home screen](readme_assets/ss1.png)

![Mods screen with one mod item selected](readme_assets/ss2.png)
Mods screen

![Mods screen with several mod items selected](readme_assets/ss3.png)
ditto with multiple items selected by Shift+click

![](readme_assets/ss4.png)
Settings screen and the Switch user dialog

![](readme_assets/ss5.png)
Mod info screen and save selection dialog (by clicking "...from save" button)

## Notice
This is a completely independent project and is not affiliated with any parties referenced throughout the program's interface.

## How Does This Work
long answer, magic

short answer, the "virtual filesystem" technology allows the launcher to construct a fake folder that replicates the traditional setup of a Ren'Py mod installation without actually consuming more disk spaces from making a copy of both the base game and the mod.<br>meaning you get the benefit of disk space efficiency AND isolated save content. 


## Dependencies
`qt6`, `pybind11`, `fuse3` (winfsp on windows)
