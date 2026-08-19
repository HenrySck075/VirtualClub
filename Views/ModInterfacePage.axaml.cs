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
using CommunityToolkit.Mvvm.Input;
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
                    Tags = new ObservableCollection<TagObject>(modEntry.Tags.ConvertAll((s) => new TagObject(s))),
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
        try
        {
            await SessionManager.LaunchAsync(balls?.Id ?? string.Empty);
        }
        catch (Exception)
        {
            return;
        }

        balls?.IsSessionRunning = true;
        SessionManager.OnSessionExitCallback(balls?.Id ?? string.Empty, () =>
        {
            balls?.IsSessionRunning = false;
        });
    }

    public async void OnStartFromSaveClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        var balls = DataContext as ModInterfacePageModel;
        var saveID = await plsjustletmedeclare.ShowSaveSelectDialog(balls!.Id);

        if (!string.IsNullOrEmpty(saveID))
        {
            try
            {
                await SessionManager.LaunchAsync(balls?.Id ?? string.Empty, selectedSaveId: saveID);
            }
            catch (Exception)
            {
                return;
            }

            balls?.IsSessionRunning = true;
            SessionManager.OnSessionExitCallback(balls?.Id ?? string.Empty, () =>
            {
                balls?.IsSessionRunning = false;
            });
        }
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

        if (await dialog.ShowAsync() == FAContentDialogResult.Primary)
        {
            ModIndexer.DeleteMod((DataContext as ModInterfacePageModel)?.Id ?? string.Empty);
            MainView.instance?.ContentFrame.GoBack();
        }
    }

    // edit flyout
    public void OnChangeDirectoryClicked()
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
    public void OnChangeIconClicked()
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

    public void OnSaveMetaEditClicked(object? sender, FAContentDialogButtonClickEventArgs e)
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

    /*
     <ContentDialog>
        <StackPanel Spacing="8" Margin="8" MinWidth="300">
            <TextBlock Text="Name"></TextBlock>
            <TextBox Text="{Binding Name, Mode=TwoWay}" />
            <TextBlock Text="Version"></TextBlock>
            <TextBox Text="{Binding Version, Mode=TwoWay}" />
            <TextBlock Text="Directory"></TextBlock>
            <Button Content="Change directory" Click="OnChangeDirectoryClicked" />
            <TextBlock Text="Icon"></TextBlock>
            <Image Width="64" Height="64" Source="{Binding Icon}" HorizontalAlignment="Center"/>
            <Button Content="Change icon" Click="OnChangeIconClicked" />

            <Button Content="Save" Click="OnSaveMetaEditClicked" HorizontalAlignment="Right" />
        </StackPanel>
    </ContentDialog>
    */

    private void CreateEditDialog()
    {
        var dialog = new FAContentDialog
        {
            Title = "Edit Mod Metadata",
            CloseButtonText = "Close",
            PrimaryButtonText = "Save",
            Content = new StackPanel
            {
                Spacing = 8,
                Margin = new Avalonia.Thickness(8),
                MinWidth = 300,
                Children =
                {
                    new TextBlock { Text = "Name" },
                    new TextBox { [!TextBox.TextProperty] = new Avalonia.Data.ReflectionBinding("Name") {Mode=Avalonia.Data.BindingMode.TwoWay} },
                    new TextBlock { Text = "Version" },
                    new TextBox { [!TextBox.TextProperty] = new Avalonia.Data.ReflectionBinding("Version") {Mode=Avalonia.Data.BindingMode.TwoWay} },
                    new TextBlock { Text = "Directory" },
                    new Button { Content = "Change directory", Command = new RelayCommand(OnChangeDirectoryClicked) },
                    new TextBlock { Text = "Icon" },
                    new Image { Width = 64, Height = 64, [!Image.SourceProperty] = new Avalonia.Data.Binding("Icon"), HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center },
                    new Button { Content = "Change icon", Command = new RelayCommand(OnChangeIconClicked) },
                }
            }
        };
        dialog.PrimaryButtonClick += OnSaveMetaEditClicked;

        dialog.ShowAsync();
    }

    private void OnEditClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        CreateEditDialog();
    }
}


/// =============================
/// =============================

internal sealed class plsjustletmedeclare
{
    public static async Task<string?> ShowSaveSelectDialog(string modId)
    {
        var entry = App.AppDataService.Index.Mods.GetValueOrDefault(modId);
        if (entry == null)
        {
            Debug.WriteLine($"Mod with ID {modId} not found.");
            return null;
        }
        var saveDir = entry.SaveDirectory;

        FAContentDialog dialog;

        string? ret = null;

        object CreateCard(string saveFile)
        {
            // Open the save file as a zip. It's a zip in disguise.
            var savePath = Path.Combine(saveDir, saveFile);
            Debug.WriteLine(savePath);
            using var archive = System.IO.Compression.ZipFile.OpenRead(savePath);

            // Read screenshot.png for thumbnail
            var screenshotEntry = archive.GetEntry("screenshot.png");
            if (screenshotEntry == null)
            {
                Debug.WriteLine($"No screenshot found in {saveFile}");
                return new TextBlock { Text = saveFile }; // Fallback to just showing the filename
            }

            using var ss = screenshotEntry.Open();
            using var stream = new MemoryStream();
            ss.CopyTo(stream);
            stream.Seek(0, SeekOrigin.Begin);
            var bitmap = new Bitmap(stream);

            // Create a card with the thumbnail and save file info
            var card = new Button
            {
                Content = new StackPanel
                {
                    Children =
                    {
                        new Image { Source = bitmap, Width = 355, Height = 200 },
                        new TextBlock { Text = saveFile }
                    }
                }
            };

            card.Click += (s, e) =>
            {
                Debug.WriteLine($"Loading save file: {saveFile}");
                ret = saveFile;
                dialog.Hide();
            };

            return card;
        }
        var stack = new StackPanel { };
        dialog = new FAContentDialog
        {
            Title = "Select a save file",
            Content = new ScrollViewer
            {
                Content = stack
            },
            CloseButtonText = "Cancel",
        };
#pragma warning disable CS8620
        stack.Children.AddRange(Directory.GetFiles(saveDir, "*-LT1.save")
                .Where(f => !Path.GetFileName(f).StartsWith("auto-") && !Path.GetFileName(f).StartsWith("quick-"))
                .Select(f => CreateCard(Path.GetFileName(f)) as Control));
#pragma warning restore CS8620
        await dialog.ShowAsync();

        return ret;
    }
}