using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using FluentAvalonia.UI.Controls;
using FluentAvalonia.UI.Navigation;
using VirtualClub.Core;

namespace VirtualClub.Views;

public record TagObject(string Name); // why 
public partial class ModInterfacePageModel : ObservableObject
{
    public string Id { get; init; } = string.Empty;
    [ObservableProperty]
    public partial string Name { get; set; }
    [ObservableProperty]
    public partial string Version { get; set; }
    [ObservableProperty]
    public partial Bitmap Icon { get; set; }
    [ObservableProperty]
    public partial string Directory { get; set; }
    public ObservableCollection<TagObject> Tags { get; init; } = new ObservableCollection<TagObject>();
    [ObservableProperty]
    public partial string PlaytimeText { get; set; }
    [ObservableProperty]
    public partial string ActivePlaytimeText { get; set; }
    [ObservableProperty]
    public partial bool IsDevMode { get; set; }
    [ObservableProperty]
    public partial bool IsForceRecompile { get; set; }
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsSessionNotRunning))]
    public partial bool IsSessionRunning { get; set; }
    public bool IsSessionNotRunning { get => !IsSessionRunning; }
}// (string Id, string Name, string Version, Bitmap IconPath, string Directory, string PlaytimeText, string ActivePlaytimeText);

public partial class ModInterfacePage : UserControl
{
    public ModInterfacePage()
    {
        InitializeComponent();

        AddHandler(FAFrame.NavigatedToEvent, OnNavigatedTo);
    }

    // every single ai collectively decided OnNavigatedTo is a real function. help.
    public void OnNavigatedTo(object? sender, FANavigationEventArgs e)
    {
        if (e.Parameter is string id)
        {
            // Load mod data based on the provided ID
            var modEntry = App.AppDataService.Index.Mods.GetValueOrDefault(id);
            if (modEntry != null)
            {
                var saveDir = modEntry.SaveDirectory;

                var playtimeFile = Path.Combine(saveDir, ".mvc_playtime"); // stored as [playtimeSeconds] [activePlaytimeSeconds]

                string playtimeText = "00:00:00";
                string activePlaytimeText = "00:00:00";

                if (File.Exists(playtimeFile))
                {
                    var playtimeData = File.ReadAllText(playtimeFile).Split(' ');
                    if (playtimeData.Length == 2 &&
                        int.TryParse(playtimeData[0], out int _playtimeSeconds) &&
                        int.TryParse(playtimeData[1], out int _activePlaytimeSeconds))
                    {
                        playtimeText = TimeSpan.FromSeconds(_playtimeSeconds).ToString(@"hh\:mm\:ss");
                        activePlaytimeText = TimeSpan.FromSeconds(_activePlaytimeSeconds).ToString(@"hh\:mm\:ss");
                    }
                }

                var model = new ModInterfacePageModel
                {
                    Id = id,
                    Name = modEntry.Name,
                    Version = modEntry.Version,
                    Icon = new Bitmap(Path.Combine(EnvironmentManager.GetDataDirectory(), "icons", modEntry.IconFilename)),
                    Directory = modEntry.Directory,
                    PlaytimeText = playtimeText,
                    Tags = new ObservableCollection<TagObject>(modEntry.Tags.ConvertAll((s)=>new TagObject(s))),
                    ActivePlaytimeText = activePlaytimeText,
                    IsSessionRunning = SessionManager.IsSessionRunning(id),
                    IsDevMode = modEntry.EnableDeveloperMode,
                    IsForceRecompile = modEntry.ForceRecompile
                };

                DataContext = model;

                // im dumb so wait for existing session to exit if one was actually running
                if (model.IsSessionRunning)
                {
                    SessionManager.OnSessionExitCallback(id, () =>
                    {
                        model.IsSessionRunning = false;
                    });
                }
            }
        }
    }

    public async void OnStartClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        var balls = DataContext as ModInterfacePageModel;
        try {
            await SessionManager.LaunchAsync((DataContext as ModInterfacePageModel)?.Id ?? string.Empty);
        } catch (Exception)
        {
            return;
        }

        balls?.IsSessionRunning = true;
        SessionManager.OnSessionExitCallback(balls?.Id ?? string.Empty, () =>
        {
            balls?.IsSessionRunning = false;
        });
    }

    public void OnStartFromSaveClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        var dialog = plsjustletmedeclare.ShowSaveSelectDialog((DataContext as ModInterfacePageModel)!.Id);
    }

    public void OnOpenVirtualFolderClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        SessionManager.OpenMountedDirectory((DataContext as ModInterfacePageModel)!.Id);
    }

    public async void OnUninstallClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        var dialog = new FAContentDialog
        {
            Title = "Are you sure wanted to delete this mod?",
            Content = "Deleting from the launcher does not mean the linked mod folder itself is deleted, however ALL other session-related data like saves will be inaccessible.",
            PrimaryButtonText = "Delete",
            CloseButtonText = "Cancel",
        };

        if (await dialog.ShowAsync() == FAContentDialogResult.Primary) {
            ModIndexer.DeleteMod((DataContext as ModInterfacePageModel)?.Id ?? string.Empty);
            MainView.instance?.ContentFrame.GoBack();
        }
    }

    // edit flyout
    public void OnChangeDirectoryClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return;

        var folders = topLevel.StorageProvider.OpenFolderPickerAsync(new Avalonia.Platform.Storage.FolderPickerOpenOptions
        {
            Title = "Select a mod directory containing a valid Ren'Py game structure",
            AllowMultiple = false
        }).ContinueWith(task =>
        {
            if (task.Result.Count > 0)
            {
                string selectedPath = task.Result[0].Path.LocalPath;
                Debug.WriteLine($"Selected directory: {selectedPath}");

                if (DataContext is ModInterfacePageModel model)
                {
                    model.Directory = selectedPath; // Update the model's directory
                }
            }
        });
    }

    // to skip doing the expensive icon changing work if the user didn't actually change the icon, we use this flag to track if the icon was changed
    private bool _isIconChanged = false;
    public void OnChangeIconClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        // do the same thing as OnChangeDirectoryClicked but for the icon file, and we dont replace the mod entry's IconFilename value

        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return;

        // thread access my ass
        var model = DataContext as ModInterfacePageModel;

        topLevel.StorageProvider.OpenFilePickerAsync(new Avalonia.Platform.Storage.FilePickerOpenOptions
        {
            Title = "Select an icon file for the mod",
            AllowMultiple = false,
            FileTypeFilter = new List<Avalonia.Platform.Storage.FilePickerFileType>
            {
                new Avalonia.Platform.Storage.FilePickerFileType("Image Files")
                {
                    Patterns = new List<string> { "*.png", "*.jpg", "*.jpeg", "*.bmp" }
                }
            }
        }).ContinueWith(task =>
        {
            if (task.Result.Count > 0)
            {
                string selectedPath = task.Result[0].Path.LocalPath;
                Debug.WriteLine($"Selected icon file: {selectedPath}");

                // Update the model's icon path
                model.Icon = new Bitmap(selectedPath);
                _isIconChanged = true;
            }
        });
    }

    public void OnSaveMetaEditClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (DataContext is ModInterfacePageModel model)
        {
            var modEntry = App.AppDataService.Index.Mods.GetValueOrDefault(model.Id);
            if (modEntry != null)
            {
                modEntry.Directory = model.Directory;
                modEntry.Name = model.Name;
                modEntry.Version = model.Version;

                if (_isIconChanged && model.Icon != null)
                {
                    // Save the new icon to the icons directory
                    string iconsDir = Path.Combine(App.AppDataService.AppDataFolder, "icons");
                    Directory.CreateDirectory(iconsDir);
                    string newIconPath = Path.Combine(iconsDir, $"{modEntry.Id}.png");

                    model.Icon.Save(newIconPath, new PngBitmapEncoderOptions());

                    LarpingUtils.Iconize(newIconPath, Path.Combine(iconsDir, $"{modEntry.Id}.icon.png"), 256, 256);

                    modEntry.IconFilename = $"{modEntry.Id}.png";
                }

                // Save the updated mods index to disk
                App.AppDataService.Save();
            }
        }
    }

    private void DevModeToggle_IsCheckedChanged(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (DataContext is ModInterfacePageModel model)
        {
            var modEntry = App.AppDataService.Index.Mods.GetValueOrDefault(model.Id);
            if (modEntry != null)
            {
                modEntry.EnableDeveloperMode = model.IsDevMode = DevModeToggle.IsChecked ?? false;
                App.AppDataService.Save();
            }
        }
    }

    private void ForceRecompileToggle_IsCheckedChanged(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (DataContext is ModInterfacePageModel model)
        {
            var modEntry = App.AppDataService.Index.Mods.GetValueOrDefault(model.Id);
            if (modEntry != null)
            {
                modEntry.ForceRecompile = model.IsForceRecompile = ForceRecompileToggle.IsChecked ?? false;
                App.AppDataService.Save();
            }
        }
    }
}


/// =============================
/// =============================

/*
python impl of this class

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

*/

/// THERE IS NO CLASS NAMED SAVESELECTDIALOG
internal sealed class plsjustletmedeclare {
    public static async Task ShowSaveSelectDialog(string modId)
    {
        var entry = App.AppDataService.Index.Mods.GetValueOrDefault(modId);
        if (entry == null)
        {
            Debug.WriteLine($"Mod with ID {modId} not found.");
            return;
        }
        var saveDir = entry.SaveDirectory;

        object CreateCard(string saveFile)
        {
            // Open the save file as a zip. It's a zip in disguise.
            var savePath = Path.Combine(saveDir, saveFile);
            using var archive = System.IO.Compression.ZipFile.OpenRead(savePath);

            // Read screenshot.png for thumbnail
            var screenshotEntry = archive.GetEntry("screenshot.png");
            if (screenshotEntry == null)
            {
                Debug.WriteLine($"No screenshot found in {saveFile}");
                return new TextBlock { Text = saveFile }; // Fallback to just showing the filename
            }

            using var stream = screenshotEntry.Open();
            var bitmap = new Bitmap(stream);

            // Create a card with the thumbnail and save file info
            var card = new Button
            {
                Content = new StackPanel
                {
                    Children =
                    {
                        new Image { Source = bitmap, Width = 200, Height = 200 },
                        new TextBlock { Text = saveFile }
                    }
                }
            };

            card.Click += (s, e) =>
            {
                Debug.WriteLine($"Loading save file: {saveFile}");
                // Implement the logic to load the selected save file
                // For example, you might call SessionManager.LaunchAsync with the appropriate parameters
            };

            return card;
        }
        var stack = new StackPanel {};
#pragma warning disable CS8620
        stack.Children.AddRange(Directory.GetFiles(saveDir, "*-LT1.save")
                .Where(f => !Path.GetFileName(f).StartsWith("auto-"))
                .Select(f => CreateCard(Path.GetFileName(f)) as Control));
#pragma warning restore CS8620
        var dialog = new FAContentDialog
        {
            Title = "Select a save file",
            Content = new ScrollViewer
            {
                Content = stack
            },
            CloseButtonText = "Cancel",
        };
        await dialog.ShowAsync();
    }
}