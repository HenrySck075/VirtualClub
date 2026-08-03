using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using FluentAvalonia.UI.Controls;
using FluentAvalonia.UI.Navigation;
using VirtualClub.Core;

namespace VirtualClub.Views;

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

    public void OnStartClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        SessionManager.LaunchAsync((DataContext as ModInterfacePageModel)?.Id ?? string.Empty);
        SessionManager.OnSessionExitCallback((DataContext as ModInterfacePageModel)?.Id ?? string.Empty, () =>
        {
            (DataContext as ModInterfacePageModel)?.IsSessionRunning = false;
        });
    }

    public void OnStartFromSaveClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        // Start the mod interface from save logic here
    }

    public void OnOpenVirtualFolderClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        // Open the virtual folder logic here
    }

    public void OnUninstallClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        ModIndexer.DeleteMod((DataContext as ModInterfacePageModel)?.Id ?? string.Empty);
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

        var files = topLevel.StorageProvider.OpenFilePickerAsync(new Avalonia.Platform.Storage.FilePickerOpenOptions
        {
            Title = "Select an icon file for the mod",
            AllowMultiple = false,
            FileTypeFilter = new List<Avalonia.Platform.Storage.FilePickerFileType>
            {
                new Avalonia.Platform.Storage.FilePickerFileType("Image Files")
                {
                    Patterns = new List<string> { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif" }
                }
            }
        }).ContinueWith(task =>
        {
            if (task.Result.Count > 0)
            {
                string selectedPath = task.Result[0].Path.LocalPath;
                Debug.WriteLine($"Selected icon file: {selectedPath}");

                if (DataContext is ModInterfacePageModel model)
                {
                    // Update the model's icon path
                    model.Icon = new Bitmap(selectedPath);
                    _isIconChanged = true;
                }
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