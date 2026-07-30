using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using FluentAvalonia.UI.Controls;
using FluentAvalonia.UI.Navigation;
using VirtualClub.Core;

namespace VirtualClub.Views;
public record ModInterfacePageModel(string Id, string Name, string VersionText, Bitmap IconPath, string Directory, string PlaytimeText, string ActivePlaytimeText);

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
        if (e.Parameter is string id) {
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

                var model = new ModInterfacePageModel(
                    id,
                    modEntry.Name,
                    $"v{modEntry.Version}",
                    new Bitmap(Path.Combine(EnvironmentManager.GetDataDirectory(), "icons", modEntry.IconFilename)),
                    modEntry.Directory,
                    playtimeText,
                    activePlaytimeText
                );

                DataContext = model;
            }
        }
    }

    public void OnStartClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        // Start the mod interface logic here
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
        // Uninstall logic here
    }
}