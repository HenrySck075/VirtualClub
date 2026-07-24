from __future__ import annotations

import json
import platform
import zipfile
import shutil
import time
import uuid
from io import BytesIO
import os, stat
from typing import Any, Union
from PySide6.QtGui import QColor, QColorConstants, QIcon, QImage
from PySide6.QtWidgets import QApplication, QCompleter, QFileDialog, QHBoxLayout, QStackedLayout
from PySide6.QtCore import QCoreApplication, QProcessEnvironment, QSettings, QSize, QStandardPaths, QTimer, Qt, Signal
from qfluentwidgets import Action, BodyLabel, CaptionLabel, CheckBox, FlowLayout, FluentIconBase, FluentWidget, FluentWindow, HorizontalSeparator, IconWidget, ImageLabel, LineEdit, MessageBox, MessageBoxBase, NavigationItemPosition, NavigationTreeWidget, PrimarySplitPushButton, PushSettingCard, RoundMenu,  ScrollArea, FluentIcon as FIF, SettingCard, SettingCardGroup, SimpleCardWidget, StrongBodyLabel, SubtitleLabel, SwitchSettingCard, TitleLabel, qrouter
from PySide6.QtWidgets import QWidget, QVBoxLayout
import sys
from PIL import Image

from qfluentwidgets.components.widgets import PushButton

from lib.rpyc_reader import peek_rpyc
from libbifuse import LibbiVFS, ActiveMount
from lib.rpa_reader import extract_single_file, read_rpa_index

def get_launcher_root():
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS # type: ignore
    except AttributeError:
        base_path = os.path.dirname(os.path.abspath(__file__))
    return base_path

QCoreApplication.setOrganizationName("MetaverseEnterprise")
QCoreApplication.setOrganizationDomain("henrysck.sh")
QCoreApplication.setApplicationName("VirtualClub")
data_dir = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.AppDataLocation)
if not os.path.exists(data_dir):
    os.makedirs(data_dir)
icons_dir = os.path.join(data_dir, "icons")
if not os.path.exists(icons_dir):
    os.makedirs(icons_dir)

def getIconPathOf(modId):
    return os.path.join(icons_dir, QSettings().value(f"{modId}/iconFilename"))
def getWindowIconPathOf(modId):
    return os.path.join(icons_dir, QSettings().value(f"{modId}/iconFilename")+".scaled.png")

def optimize_window_icon(input_path, output_path, size=(256, 256)):
    # 1. Open the original image
    with Image.open(input_path) as img:
        # Ensure it has an alpha channel for transparency
        img = img.convert("RGBA")
        
        # 2. Downscale using LANCZOS for high-quality resizing
        # (Replaces the deprecated ANTIALIAS)
        icon_img = img.resize(size, Image.Resampling.LANCZOS)
        
        # 3. Optimize the file size
        # Converting to 'P' (Palette) mode with an adaptive palette and alpha 
        # dramatically drops the PNG file size (often by 70%+) compared to raw RGBA.
        optimized_img = icon_img.quantize(colors=256, method=Image.Quantize.FASTOCTREE)
        
        # 4. Save with maximum PNG compression
        optimized_img.save(output_path, "PNG", optimize=True, compress_level=9)
        print(f"Icon saved successfully to {output_path}!")

class GeneralConfigInterface(ScrollArea):
    def __init__(self):
        super().__init__()
        self.enableTransparentBackground()
        self.setWidgetResizable(True)
        self.setWidget(self.createContent())
        self.setObjectName("mainInterface")
        
        self.settings = QSettings()
        self.folder = self.settings.value("baseGameInstallation")
        self.updateInstallationFolder(None)

    def createContent(self):
        content = QWidget()
        content.setObjectName("contentWidget")
        content.setStyleSheet("#contentWidget { background-color: transparent; }")
        layout = QVBoxLayout(content)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        layout.setContentsMargins(36, 24, 36, 24)
        layout.setSpacing(24)
        layout.addWidget(TitleLabel("yo wassup"))
        layout.addWidget(HorizontalSeparator())

        baseGameFolderLine = QWidget()
        layout2 = QHBoxLayout(baseGameFolderLine)
        layout2.setAlignment(Qt.AlignmentFlag.AlignTop)
        button = PushButton(FIF.FOLDER, "Change folder")
        def bgfButtonCallback():
            self.updateInstallationFolder(QFileDialog.getExistingDirectory(self, "Select extracted DDLC installation folder (NOT THE STEAM ONE)"))
        button.clicked.connect(bgfButtonCallback)
        self.bgfText = BodyLabel("DDLC installation folder: ")
        layout2.addWidget(self.bgfText)
        layout2.addWidget(button)

        layout.addWidget(baseGameFolderLine)

        return content


    def updateInstallationFolder(self, folder):
        if folder:
            self.folder = folder
            self.settings.setValue("baseGameInstallation", folder)
        self.bgfText.setText(f"DDLC installation folder: {self.folder or ''}")

class AddModDialog(MessageBoxBase):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add new mod")
        self.titleLabel = SubtitleLabel("Add new mod")

        self.folderSelectLabel = BodyLabel("Select a mod directory containing a _valid Ren'Py game structure_ to add.")
        self.extractedFolderLabel = BodyLabel("")
        
        selectFolderButton = PushButton(FIF.FOLDER_ADD, "click me :3")

        self.selectedFolder: str|None = None

        def selectFolderCallback():
            folder = QFileDialog.getExistingDirectory(self, "Select a mod directory containing a _valid Ren'Py game structure_ to add.")
            if folder:
                self.extractedFolderLabel.setText(f"Selected folder: {folder}")
                self.selectedFolder = folder
        selectFolderButton.clicked.connect(selectFolderCallback)

        self.viewLayout.addWidget(self.titleLabel)
        self.viewLayout.addWidget(self.folderSelectLabel)
        self.viewLayout.addWidget(self.extractedFolderLabel)
        self.viewLayout.addWidget(selectFolderButton)

    def validate(self):
        if not self.selectedFolder:
            return False
        mainWindow: MainWindow = self.parent() # type: ignore
        mainWindow.modAddEvent.emit(self.selectedFolder)
        return True

class PrimarySplitPushSettingCard(SettingCard):
    """ Setting card with a primary split push button """

    clicked = Signal()

    def __init__(self, text, menu: RoundMenu, icon: Union[str, QIcon, FluentIconBase], title, content=None, parent=None):
        """
        Parameters
        ----------
        text: str
            the text of push button
        
        menu: RoundMenu
            the menu to be attached to the push button

        icon: str | QIcon | FluentIconBase
            the icon to be drawn

        title: str
            the title of card

        content: str
            the content of card

        parent: QWidget
            parent widget
        """
        super().__init__(icon, title, content, parent)
        self.button = PrimarySplitPushButton(text, self)
        #menu.setParent(self.button)
        self.button.setFlyout(menu)
        self.hBoxLayout.addWidget(self.button, 0, Qt.AlignmentFlag.AlignRight)
        self.hBoxLayout.addSpacing(16)
        self.button.clicked.connect(self.clicked)

def format_duration(seconds: int):
    "format: DD days, HH hours, MM minutes and SS seconds"
    days, seconds = divmod(seconds, 86400)
    hours, seconds = divmod(seconds, 3600)
    minutes, seconds = divmod(seconds, 60)
    
    parts = []

    if days > 0:
        parts.append(f"{days} days")
    if hours > 0:
        parts.append(f"{hours} hours")
    if minutes > 0:
        parts.append(f"{minutes} minutes")
    if seconds > 0 or not parts:
        parts.append(f"{seconds} seconds")

    return ", ".join(parts)

class ModInterface(ScrollArea):
    def __init__(self, window: MainWindow, modId):
        super().__init__()
        self.enableTransparentBackground()
        self.settings = QSettings()
        self.modId = modId
        self.mainWindow = window
        
        self.name = self.settings.value(f"{modId}/name")
        self.buildId = self.settings.value(f"{modId}/buildId")
        self.version = self.settings.value(f"{modId}/version")
        self.iconFilename = self.settings.value(f"{modId}/iconFilename")
        self.folder = self.settings.value(f"{modId}/directory")
        self.developerEnabled:bool = self.settings.value(f"{modId}/developerMode", defaultValue=False, type=bool) # type: ignore

        self.setObjectName(modId+self.version)
        self.setWidgetResizable(True)
        
        # Win11 Settings uses transparent backgrounds for scroll containers
        # self.setStyleSheet("QScrollArea { background-color: transparent; border: none; }")
        self.setWidget(self.createContent())
        self.fuse: ActiveMount|None = None

    def createContent(self):
        content = QWidget()
        content.setObjectName("contentWidget")
        content.setStyleSheet("#contentWidget { background-color: transparent; }")
        
        layout = QVBoxLayout(content)
        layout.setContentsMargins(36, 24, 36, 24)
        layout.setSpacing(24)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        # -------------------------------------------------------------------
        # Header Area (Title & Subtitle)
        # -------------------------------------------------------------------
        headerWidget = QWidget()
        headerLayout = QHBoxLayout(headerWidget)
        headerLayout.setAlignment(Qt.AlignmentFlag.AlignLeft)
        headerLayout.setContentsMargins(0, 0, 0, 0)
        headerLayout.setSpacing(16)

        titleWidget = QWidget()
        titleLayout = QVBoxLayout(titleWidget)
        titleLayout.setContentsMargins(0, 0, 0, 0)
        titleLayout.setSpacing(2)
        
        titleLabel = TitleLabel(self.name)
        versionLabel = BodyLabel(f"Version {self.version}")
        versionLabel.setTextColor(QColor(Qt.GlobalColor.gray), QColor(Qt.GlobalColor.darkGray))
        directoryLabel = BodyLabel(f"Directory: {self.folder}")
        directoryLabel.setTextColor(QColor(Qt.GlobalColor.gray), QColor(Qt.GlobalColor.darkGray))
        metaEditButton = PushButton(FIF.EDIT, "Edit")
        metaEditButton.clicked.connect(lambda: ModEditDialog(self.modId, self, self.mainWindow).exec())
        
        titleLayout.addWidget(titleLabel)
        titleLayout.addWidget(versionLabel)
        titleLayout.addWidget(directoryLabel)
        titleLayout.addWidget(metaEditButton)

        # the mod icon
        iconWidget = IconWidget(QIcon(os.path.join(icons_dir,self.iconFilename)))
        iconSize = 120
        iconWidget.setFixedSize(iconSize, iconSize)
        headerLayout.addWidget(iconWidget)
        headerLayout.addWidget(titleWidget)
        
        layout.addWidget(headerWidget)
        layout.addWidget(HorizontalSeparator())

        # -------------------------------------------------------------------
        # Straight bs
        # -------------------------------------------------------------------
        ptf = os.path.join(self.get_save_dir(), ".mvc_playtime")
        if os.path.exists(ptf):
            playtimeLine = QWidget(styleSheet="background-color: transparent")
            playtimeLayout = QHBoxLayout(playtimeLine)
            playtimeLayout.setAlignment(Qt.AlignmentFlag.AlignLeft)
            i = IconWidget(FIF.DATE_TIME)
            i.setFixedSize(20,20) #?
            playtimeLayout.addWidget(i)

            with open(ptf, "r") as f:
                h = f.read().split()
                playtime_raw = int(h[0]) # in
                playtime_active = int(h[1]) # seconds
            
            playtimeLayout.addWidget(BodyLabel(f"Session time: {format_duration(playtime_raw)} (Active: {format_duration(playtime_active)})"))
            
            layout.addWidget(playtimeLine)
    

        # -------------------------------------------------------------------
        # Setting Group & Fluent Cards
        # -------------------------------------------------------------------
        modActionsGroup = SettingCardGroup("Mod management", content)
        
        # Primary Action Card (Prominent Accent Start Button)
        startCardMenu = RoundMenu()
        startCardMenu.addAction(Action(FIF.SAVE, "Start from save", triggered=self.onStartModFromSave))

        self.startCard = PrimarySplitPushSettingCard(
            text="Start",
            menu=startCardMenu,
            icon=FIF.PLAY,
            title="Launch application",
            content="Run this Ren'Py mod configuration environment.",
            parent=modActionsGroup
        )
        self.startCard.clicked.connect(self.onStartMod)

        self.devModeCard = SwitchSettingCard(FIF.DEVELOPER_TOOLS, "Developer mode", "Toggle developer tools in the game.",parent=modActionsGroup)
        self.devModeCard.setChecked(self.developerEnabled)
        # yeah theres configItem ik
        self.devModeCard.checkedChanged.connect(self.onSetDevMode)
        
        # Secondary Action Card
        self.uninstallCard = PushSettingCard(
            text="Uninstall",
            icon=FIF.DELETE,
            title="Remove mod",
            content="Delete mod deployment indices, and meta configs.",
            parent=modActionsGroup
        )
        self.uninstallCard.clicked.connect(self.onUninstallMod)

        # Add cards to the group container
        modActionsGroup.addSettingCard(self.startCard)
        modActionsGroup.addSettingCard(self.devModeCard)
        modActionsGroup.addSettingCard(self.uninstallCard)
        
        layout.addWidget(modActionsGroup)
        return content

    def onSetDevMode(self, enabled: bool):
        self.developerEnabled = enabled
        self.settings.setValue(f"{self.modId}/developerMode", enabled)
        self.settings.sync()
    
    def onStartMod(self):
        self.launchMod()

    def launchMod(self, extraEnvs = {}):
        print(f"[ModInterface] Launching mod session via FUSE pipeline: {self.name} (ID: {self.modId})")

        def setState(bo: bool):
            self.startCard.button.setDisabled(bo)
            self.devModeCard.switchButton.setDisabled(bo)
            self.uninstallCard.button.setDisabled(bo)
        setState(True)


        import platform
        import tempfile
        import os
        from PySide6.QtCore import QProcess

        try:
            # Create a unique, platform-agnostic temporary directory to serve as the mount point
            # tempfile.mkdtemp handles the distinct permission structures of Windows, macOS, and Linux cleanly.
            mountdir = tempfile.mkdtemp(prefix=f"renpy_mod_{self.modId}_")

            launcherRoot = get_launcher_root()
            print(launcherRoot)

            # magic, but in a nutshell its a delta vfs. exists until the game process exits
            if self.fuse != None: self.fuse.unmount()
            self.fuse = fuse = ActiveMount()
            vfs = LibbiVFS(
                    launcherRoot, self.settings.value("baseGameInstallation"), self.folder,
                )
            fuse.mount(
                mountdir, 
                vfs 
            )
            #vfs.enableYapping()

            # get the architecture to determine where to look for the python under lib/
            arch, os_type = platform.architecture()[0], platform.system()
            
            python_exc = None

            def set_python_exc(plat):
                nonlocal python_exc
                pythonw_exc = "pythonw" + (".exe" if plat.startswith("windows") else "")
                # python3 (renpy 8)
                python_exc = os.path.join(mountdir, "lib", f"py3-{plat}", pythonw_exc)
                if not os.path.exists(python_exc):
                    # fallback to python2 (renpy 7)
                    python_exc = os.path.join(mountdir, "lib", f"py2-{plat}", pythonw_exc)
                    if not os.path.exists(python_exc):
                        python_exc = os.path.join(mountdir, "lib", plat, pythonw_exc)

            if os_type == "Linux":
                if arch == "64bit":
                    set_python_exc("linux-x86_64")
                elif arch == "32bit":
                    set_python_exc("linux-i686")
            elif os_type == "Windows":
                if arch == "64bit":
                    set_python_exc("windows-x86_64")
                elif arch == "32bit":
                    set_python_exc("windows-i686")
            elif os_type == "Darwin":
                python_exc = os.path.join(mountdir, f"{self.modId}.app", "Contents", "MacOS", "pythonw")
            
            def cleanup():
                try:
                    fuse.unmount()
                    os.rmdir(mountdir)
                    self.fuse = None
                except Exception as e:
                    print(f"[ModInterface] Cleanup warning: {e}")
                finally:
                    setState(False)


            if not python_exc or not os.path.exists(python_exc):
                print(python_exc)
                cleanup()
                raise FileNotFoundError(f"Could not find a valid Python executable for {os_type} {arch}.")
            
            # Date to Dream Of is one of the release whose linux python executables got their execute bit wiped out somehow
            if os_type == "Linux":
                st = os.stat(python_exc)
                if not (st.st_mode & stat.S_IXUSR):
                    print(f"Adding executable permissions to {python_exc}")
                    # Retain current permissions but add user, group, and other execute permissions
                    os.chmod(python_exc, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

            # Launch via QProcess so the PySide GUI remains responsive and can track life cycle
            self.process = QProcess()
            env = QProcessEnvironment.systemEnvironment()
            env.insert("MVC_MOD_ID", self.modId)
            for key, value in extraEnvs.items():
                env.insert(key, value)

            # renpy 6 stuff
            if os_type == "Linux":
                current_ld = env.value("LD_LIBRARY_PATH", "")
                new_path = os.path.dirname(python_exc)
                if current_ld:
                    combined_ld = f"{new_path}:{current_ld}"
                else:
                    combined_ld = new_path
                env.insert("LD_LIBRARY_PATH", combined_ld)

            if self.developerEnabled:
                env.insert("MVC_DEVELOPER", "Just.... uh wait who was that girl's name again? Mon-ika? why is she a squid?")

            self.process.setProcessEnvironment(env)
            self.process.setWorkingDirectory(mountdir)
            printPipe = lambda d, color="": print(f"{color}{d.data().decode('utf-8', errors='replace')}\033[0m", end="") if color else print(d.data().decode('utf-8', errors='replace'), end="")
            self.process.readyReadStandardOutput.connect(lambda: printPipe(self.process.readAllStandardOutput(), color="\033[31m"))
            self.process.readyReadStandardError.connect(lambda: printPipe(self.process.readAllStandardError()))
                
            # Ensure cleanup triggers immediately when the game closes
            def handle_finish(exit_code, exit_status):
                print(f"[ModInterface] Process exited with code {exit_code} {exit_status}.")
                if exit_code == 0: cleanup() # leave the folder up for error checking
                setState(False)
            self.process.finished.connect(handle_finish)

            args = []
            # if the mod is a Ren'Py 6 mod (easiest detection is no lib/ in the mod folder, there could be edge case but we'll deal with that later)
            # add the -EO flag first. idk how that makes python able to find its libs but without the flag it cant.
            if not os.path.exists(os.path.join(self.folder, "lib")):
                args.append("-EO")
            bootstrapperFile = f"{self.buildId}.py"
            if not os.path.exists(os.path.join(mountdir, bootstrapperFile)):
                bootstrapperFile = "DDLC.py"
            args.append(os.path.join(mountdir, bootstrapperFile))
            self.process.start(python_exc, args)
        except Exception:
            setState(False)
            raise

    def onUninstallMod(self):
        if MessageBox(
            "Are you sure you want to uninstall this mod?",
            "Removing it this way DOES NOT delete the selected mod folders, BUT it will delete all the save data! (unless i catched the wrong folder but whatever)",
            self.mainWindow
        ).exec():
            pass
        else: return
        print(f"[ModInterface] Purging metadata entries for mod environment: {self.modId}")
        self.mainWindow.mods.remove(self.modId)
        allKeys = self.settings.allKeys()
        for key in allKeys:
            if key.startswith(f"{self.modId}/"):
                self.settings.remove(key)

        # remove that one .libbivfs folder inside the mod folder created by the fuse module if it exists
        meta_folder = os.path.join(self.folder, ".libbivfs")
        if os.path.exists(meta_folder):
            shutil.rmtree(meta_folder)
        # and the icon image
        icon_path = os.path.join(icons_dir, self.iconFilename)
        if os.path.exists(icon_path):
            os.remove(icon_path)

        # and the game's renpy saves. named after the mod id
        save_dir = self.get_save_dir()
        if os.path.exists(save_dir):
            shutil.rmtree(save_dir)

        self.mainWindow.saveModsList()
        
        # Remove dynamically
        self.mainWindow.removeInterface(self)
        if self in self.mainWindow.navs:
            self.mainWindow.navs.remove(self)
            
        if self.mainWindow.navs:
            self.mainWindow.switchTo(self.mainWindow.navs[0])
        else:
            self.mainWindow.switchTo(self.mainWindow.generalInterface)
            
        self.settings.sync()

    def get_save_dir(self):        
        if platform.system() == "Windows":
            save_dir = os.path.join(os.environ.get("APPDATA", ""), "RenPy", self.modId)
        elif platform.system() == "Darwin":
            save_dir = os.path.join(os.path.expanduser("~"), "Library", "Application Support", "RenPy", self.modId) # TODO: i dont have a mac can sb check this
        else: # Linux and other Unix-like systems
            save_dir = os.path.join(os.path.expanduser("~"), ".renpy", self.modId)

        if not os.path.exists(save_dir):
            save_dir = os.path.join(self.folder, "game", "saves") # the fallback path

        return save_dir

    def onStartModFromSave(self):
        w = SaveSelectWindow(self)
        w.show()

    def onActuallyStartModFromSave(self, save_id):
        self.launchMod(extraEnvs={"MVC_SAVE_ID": save_id})

class SaveSelectWindow(FluentWidget):
    def __init__(self, modInterface: ModInterface, parent=None):
        super().__init__(parent)
        self.modInterface = modInterface
        self.setBackgroundColor(QColorConstants.Transparent)
        self.setWindowTitle("Select a save file")
        self.setMinimumSize(700, 500)
        self.setWindowIcon(QIcon(getWindowIconPathOf(modInterface.modId)))
        self.save_dir = modInterface.get_save_dir()
        # TODO: while it has always been -LT1.save for the versions that i checked, its better to scan the engine files for the actual answer
        # on the rare case the savegame_suffix ever be dynamically changed, well screw us ig
        self.save_files = [f for f in os.listdir(self.save_dir) if f.endswith("-LT1.save")]
        self.initUI()

    def createCard(self, save_file):
        # open the save file as zip. yeah its a zip in disguise
        save_path = os.path.join(self.save_dir, save_file)
        z = zipfile.ZipFile(save_path, 'r')

        width = height = 200

        card = SimpleCardWidget(self)
        card.setFixedSize(width, height)
        layout = QStackedLayout(card, stackingMode=QStackedLayout.StackingMode.StackAll)

        # Background: the thumbnal
        # read screenshot.png for thumbnail
        screenshot_data = z.read('screenshot.png')
        image = QImage.fromData(screenshot_data)
        image = image.scaled(QSize(width,height), Qt.AspectRatioMode.KeepAspectRatioByExpanding,Qt.TransformationMode.SmoothTransformation)
        # 2. Calculate coordinates to pull out the center block
        x = (image.width() - width) // 2
        y = (image.height() - height) // 2
        
        # 3. Crop to exact target size
        image = image.copy(x, y, width, height)
        screenshot_widget = ImageLabel(image)
        screenshot_widget.setFixedSize(width, height)
        r:int = card.getBorderRadius()
        screenshot_widget.setBorderRadius(r,r,r,r)
        layout.addWidget(screenshot_widget)

        # text
        # with a gradient
        content = QWidget(styleSheet=f"background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(0, 0, 0, 0), stop:1 rgba(0, 0, 0, 200)); border-radius: {r}px;")
        content_layout = QVBoxLayout(content)
        content_layout.setAlignment(Qt.AlignmentFlag.AlignBottom)
        content_layout.setContentsMargins(8, 8, 8, 8)
        # name
        j = json.load(z.open("json"))
        save_name = j["_save_name"] or save_file.removesuffix("-LT1.save")
        name_label = StrongBodyLabel(save_name)
        name_label.setTextColor(QColorConstants.White)
        content_layout.addWidget(name_label)

        # date in dd/mm/yyyy
        date = time.strftime("%d/%m/%Y", time.localtime(os.path.getmtime(save_path)))
        date_label = CaptionLabel(date)
        date_label.setTextColor(QColorConstants.White)
        content_layout.addWidget(date_label)
        layout.addWidget(content)
        layout.setCurrentWidget(content)


        card.setClickEnabled(True)
        def card_callback():
            self.modInterface.launchMod(extraEnvs={"MVC_SAVE_ID": save_file.removesuffix("-LT1.save")})
            self.close()
        card.clicked.connect(card_callback)

        return card
    
    
    def initUI(self):
        layout = QVBoxLayout(self) # type: ignore # QLayout: Attempting to add QLayout "" to SaveSelectWindow "", which already has a layout
        layout.setContentsMargins(0, self.titleBar.height(), 0, 0)
 
        self.save_list_widget = ScrollArea()
        self.save_list_widget.setObjectName("ba")
        self.save_list_widget.setStyleSheet("QScrollArea#ba { background-color: transparent; border: none; }")
        self.save_list_widget.setWidgetResizable(True)
        save_list_content = QWidget(styleSheet="background-color: transparent;")
        save_list_layout = FlowLayout(save_list_content)
        #save_list_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        for save_file in self.save_files:
            if save_file.startswith("auto-"): continue
            save_button = self.createCard(save_file)
            #save_button.clicked.connect(lambda checked, sf=save_file: self.load_save(sf))
            save_list_layout.addWidget(save_button)

        self.save_list_widget.setWidget(save_list_content)
        layout.addWidget(self.save_list_widget)

    def load_save(self, save_file):
        print(f"Loading save file: {save_file}")
        # Implement the logic to load the selected save file

def get_rpyc_statements(unpickled_data):
    if isinstance(unpickled_data, tuple) and len(unpickled_data) == 2:
        _, statements = unpickled_data
    else:
        statements = unpickled_data

    return statements

class MainWindow(FluentWindow):
    def insertSubInterface(self, index: int, interface: QWidget, icon: Union[FluentIconBase, QIcon, str], text: str,
                        position=NavigationItemPosition.TOP, parent=None, isTransparent=False) -> NavigationTreeWidget:
        """ insert sub interface, the object name of `interface` should be set already
        before calling this method

        need to be completely honest with ya idk why he doesn't have this function in the class

        Parameters
        ----------
        interface: QWidget
            the subinterface to be added

        icon: FluentIconBase | QIcon | str
            the icon of navigation item

        text: str
            the text of navigation item

        position: NavigationItemPosition
            the position of navigation item

        parent: QWidget | str
            * QWidget: the parent of navigation item
            * str: the parent route key of navigation item

        isTransparent: bool
            whether to use transparent background
        """
        if not interface.objectName():
            raise ValueError("The object name of `interface` can't be empty string.")

        parentRouteKey = parent
        if parent and isinstance(parent, QWidget):
            parentRouteKey = parent.objectName()
            if not parentRouteKey:
                raise ValueError("The object name of `parent` can't be empty string.")

        interface.setProperty("isStackedTransparent", isTransparent)
        self.stackedWidget.view.insertWidget(index,interface)

        # add navigation item
        routeKey = interface.objectName()
        item = self.navigationInterface.insertItem(
            index=index,
            routeKey=routeKey,
            icon=icon,
            text=text,
            onClick=lambda: self.switchTo(interface),
            position=position,
            tooltip=text,
            parentRouteKey=parentRouteKey # type: ignore
        )

        # initialize selected item
        if self.stackedWidget.count() == 1:
            self.stackedWidget.currentChanged.connect(self._onCurrentInterfaceChanged)
            self.navigationInterface.setCurrentItem(routeKey)
            qrouter.setDefaultRouteKey(self.stackedWidget, routeKey) # type: ignore

        self._updateStackedBackground()

        return item


    # Create the mod add event. used by the AddModDialog on its validate() function (which is an excuse to send data on ok button)
    modAddEvent = Signal(str, name="balls")
    def __init__(self):
        super().__init__()
        self.settings = QSettings()
        self.modAddEvent.connect(self.onAddNewModRequest)
        self.navigationInterface.setReturnButtonVisible(False)

        self.mods = list[str]()
        self.loadModsList()

        self.navs: list[QWidget] = []
        #self.navigationInterface.setAcrylicEnabled(True)
        self.initNavigation()

        # set a bigger windo size   
        self.resize(900, 700)

    def onAddNewModRequest(self, directory: str):
        gamedir = os.path.join(directory, "game")
        # build a complete index from every single .rpa files existing
        indexes = {} # dict[archiveFile, index]
        def walktuah(*paths):
            for p in paths: yield from os.walk(p)
        # walk both the mod's game directory and the base game directory to find all .rpa files
        for root, dirs, files in walktuah(gamedir, os.path.join(self.settings.value("baseGameInstallation"),"game")):
            for file in files:
                if file.endswith(".rpa"):
                    archiveFile = os.path.join(root, file)
                    index = read_rpa_index(archiveFile)
                    indexes[archiveFile] = index

        def read_game_file(filepath):
            if os.path.exists(os.path.join(gamedir, filepath)):
                with open(os.path.join(gamedir, filepath), "rb") as f:
                    return f.read()
            else:
                for arc, index in indexes.items():
                    if filepath in index:
                        return extract_single_file(arc, filepath, index)
        # check for the unarchived state: look for options.rpyc
        balls = BytesIO(read_game_file("options.rpyc")) # type: ignore
        statements = get_rpyc_statements(peek_rpyc(balls))
        statements = statements if isinstance(statements, list) else [statements]

        requested_defines = {
            "config": ["name", "window_icon", "version", "save_directory"],
            "build": ["name"]
        }
        defines = {}
        def lookup_defines(stmts):
            for statement in stmts:
                node_type = type(statement).__name__
                if node_type == "Init" and hasattr(statement, 'block'):
                    lookup_defines(statement.block)
                elif node_type in ("Define", "Default"):
                    store = getattr(statement, "store", "store")
                    store = store.removeprefix("store.")
                    if store == "store": store = ""

                    if store in requested_defines:
                        varname = getattr(statement, "varname", "")
                        if varname in requested_defines[store]:
                            code_obj = getattr(statement, "code", None)
                            code_str = getattr(code_obj, "source", str(code_obj))

                            print(code_str)

                            defines[(store+"." if store != "" else "")+varname] = eval(code_str) # bit scary but what am i supposed to do
        lookup_defines(statements)
        print(defines)
        
        name = defines.get("config.name", "Doki Doki Literature Club!") # this rarely happens
        version = defines.get("config.version", "1.0.0")
        icon = defines.get("config.window_icon", "").removeprefix("/")
        buildId = defines.get("build.name", "DDLC")
        saveDirectory = defines.get("config.save_directory", "DDLC")

        seed_string = f"{buildId}{name}{saveDirectory}{time.time()}"
        mod_uuid = str(uuid.uuid5(uuid.NAMESPACE_DNS, seed_string))

        if not icon: icon = os.path.join(get_launcher_root(), "logoplacehold.jpg")
        icon_content = read_game_file(icon)
        icon_ext = os.path.splitext(icon)[1]

        # icon path helper functions can't be used here since the configs doesn't exist yet
        if icon_content is not None:
            with open(os.path.join(icons_dir,mod_uuid+icon_ext), "wb") as f:
                f.write(icon_content)

            optimize_window_icon(os.path.join(icons_dir,mod_uuid+icon_ext), os.path.join(icons_dir,mod_uuid+".scaled.png"))
            

        self.writeEntry(mod_uuid, {
            "name": name, 
            "buildId": buildId,# used to lookup the bootstrapper file
            "directory": directory, 
            "version": version, 
            "iconFilename": mod_uuid+icon_ext,
            "originalIconPath": "/"+icon
        })

        interface = ModInterface(self, mod_uuid)
        self.navs.append(interface)
        self.addSubInterface(interface, QIcon(os.path.join(icons_dir, mod_uuid+icon_ext)), f"{name} ({version})", NavigationItemPosition.SCROLL)
        self.switchTo(interface)
        
        self.settings.sync()
    
    def saveModsList(self):
        self.settings.beginWriteArray("mods")
        for i, buildId in enumerate(self.mods):
            self.settings.setArrayIndex(i)
            self.settings.setValue("b", buildId)
        self.settings.endArray()
    def loadModsList(self):
        self.mods.clear()
        size = self.settings.beginReadArray("mods")
        for i in range(size):
            self.settings.setArrayIndex(i)
            buildId = self.settings.value("b")
            if buildId:
                self.mods.append(buildId.strip())
        self.settings.endArray()

    def initNavigation(self):
        self.generalInterface = GeneralConfigInterface()
        self.addSubInterface(self.generalInterface, FIF.SETTING, "General")
        
        for i in self.mods:
            name = self.settings.value(f"{i}/name")
            version = self.settings.value(f"{i}/version")
            iconFilename = self.settings.value(f"{i}/iconFilename")

            if name and version and iconFilename:
                interface = ModInterface(self, i)
                self.navs.append(interface)
                self.addSubInterface(interface, QIcon(os.path.join(icons_dir,iconFilename)), f"{name} ({version})", NavigationItemPosition.SCROLL)

        self.navigationInterface.addItem("sixswan", FIF.ADD, "Add new mod", self.onAddNewMod, position=NavigationItemPosition.BOTTOM)

    def writeEntry(
        self, buildId, 
        configs: dict[str, Any]
    ):
        self.mods.append(buildId)
        self.saveModsList()

        for key, value in configs.items():
            self.settings.setValue(f"{buildId}/{key}", value)
    def onAddNewMod(self):
        AddModDialog(self).exec()
    def close(self, /) -> bool:
        return super().close()

class ModEditDialog(MessageBoxBase):
    def __init__(self, modId, modInterface: ModInterface, parent: MainWindow):
        super().__init__(parent)
        self.setWindowTitle("Edit mod metadata")
        self.titleLabel = SubtitleLabel("Edit mod metadata")

        # minimum reasonable width
        self.widget.setMinimumWidth(400)

        self.modId = modId
        self.modInterface = modInterface

        self.nameLineEdit = LineEdit()
        self.nameLineEdit.setText(modInterface.name)
        self.versionLineEdit = LineEdit() 
        self.versionLineEdit.setText(modInterface.version)
        self.iconChangeButton = PushButton(FIF.EDIT, "Change icon")
        self.iconChangeButton.clicked.connect(self.onChangeIcon)
        self.iconImage = ImageLabel(QIcon(getIconPathOf(modId)).pixmap(64,64))
        self.directoryChangeButton = PushButton(FIF.FOLDER, "Change directory")
        self.directoryChangeButton.clicked.connect(self.onChangeDirectory)
        self.currentDirLabel = BodyLabel(self.modInterface.folder)

        self.viewLayout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.viewLayout.addWidget(self.titleLabel)
        self.viewLayout.addWidget(BodyLabel("Name:"))
        self.viewLayout.addWidget(self.nameLineEdit)
        self.viewLayout.addWidget(BodyLabel("Version:"))
        self.viewLayout.addWidget(self.versionLineEdit)
        self.viewLayout.addWidget(BodyLabel("Icon:"))
        self.viewLayout.addWidget(self.iconImage)
        self.viewLayout.addWidget(self.iconChangeButton)
        self.viewLayout.addWidget(BodyLabel("Directory:"))
        self.viewLayout.addWidget(self.currentDirLabel)
        self.viewLayout.addWidget(self.directoryChangeButton)
 
    def onChangeDirectory(self):
        new_directory = QFileDialog.getExistingDirectory(self, "Select new mod directory")
        if new_directory:
            self.currentDirLabel.setText(new_directory)

    def onChangeIcon(self):
        self.new_icon_path, _ = QFileDialog.getOpenFileName(self, "Select new icon", "", "Images (*.png *.jpg *.jpeg *.bmp)")
        if self.new_icon_path:
            
            # Update the icon image in the dialog
            self.iconImage.setPixmap(QIcon(self.new_icon_path).pixmap(64, 64))

    # an excuse to apply the changes
    def validate(self):
        new_name = self.nameLineEdit.text()
        new_version = self.versionLineEdit.text()
        new_directory = self.currentDirLabel.text()

        # Update the settings with the new values
        settings = QSettings()
        settings.setValue(f"{self.modId}/name", new_name)
        settings.setValue(f"{self.modId}/version", new_version)
        settings.setValue(f"{self.modId}/directory", new_directory)
        if hasattr(self, "new_icon_path"):
            new_icon_filename = self.modId + os.path.splitext(self.new_icon_path)[1]
            settings.setValue(f"{self.modId}/iconFilename", new_icon_filename)
            self.modInterface.iconFilename = new_icon_filename
            # copy the image file over
            dest_icon_path = os.path.join(icons_dir, new_icon_filename)
            shutil.copyfile(self.new_icon_path, dest_icon_path)
            new_scaled_icon_path = os.path.join(icons_dir, self.modId + ".scaled.png")
            optimize_window_icon(self.new_icon_path, new_scaled_icon_path)

        # Update the mod interface with the new values
        self.modInterface.name = new_name
        self.modInterface.version = new_version
        self.modInterface.folder = new_directory
        self.modInterface.setObjectName(self.modId + new_version)

        mainWindow: MainWindow = self.parent()  # type: ignore
        
        # Replace the QTimer reload/switchTo logic with targeted interface insertion
        index = mainWindow.stackedWidget.indexOf(self.modInterface)
        mainWindow.removeInterface(self.modInterface)
        
        mainWindow.insertSubInterface(
            index=index,
            interface=self.modInterface,
            icon=QIcon(os.path.join(icons_dir, self.modInterface.iconFilename)),
            text=f"{new_name} ({new_version})",
            position=NavigationItemPosition.SCROLL
        )
        mainWindow.switchTo(self.modInterface)
        
        settings.sync()
        return True

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setAttribute(Qt.ApplicationAttribute.AA_DontCreateNativeWidgetSiblings)
    w = MainWindow()
    w.show()
    app.exec()
