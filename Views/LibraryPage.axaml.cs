using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Interactivity;
using FluentAvalonia.UI.Controls;
using FluentAvalonia.UI.Media.Animation;
using VirtualClub.Core;

namespace VirtualClub.Views;

public record ModCardViewModel(string Id, string Name, string VersionText, Uri IconPath);

public sealed class LibraryPageViewModel
{
    public ObservableCollection<ModCardViewModel> AllMods { get; } = new();

    public LibraryPageViewModel()
    {
        LoadMods();

        App.AppDataService.Index.Mods.CollectionChanged += (sender, args) =>
        {
            if (args.NewItems != null)
            {
                foreach (var newItem in args.NewItems)
                {
                    if (newItem is KeyValuePair<string, ModEntry> kvp)
                    {
                        _addMod(kvp.Key, kvp.Value);
                    }
                }
            }

            if (args.OldItems != null)
            {
                foreach (var oldItem in args.OldItems)
                {
                    if (oldItem is KeyValuePair<string, ModEntry> kvp)
                    {
                        _removeMod(kvp.Key);
                    }
                }
            }

            //FilterMods();
        };
    }

    private void _addMod(string id, ModEntry modEntry)
    {
        string name = modEntry.Name;
        string version = modEntry.Version;
        string iconFilename = modEntry.IconFilename;
        Uri iconPath = new Uri(Path.Combine(EnvironmentManager.GetDataDirectory(), "icons", iconFilename));

        AllMods.Add(new ModCardViewModel(id, name, $"v{version}", iconPath));
    }

    private void _removeMod(string id)
    {
        var modToRemove = AllMods.FirstOrDefault(m => m.Id == id);
        if (modToRemove != null)
        {
            AllMods.Remove(modToRemove);
        }
    }


    public void LoadMods()
    {
        AllMods.Clear();

        var mods =  App.AppDataService.Index.Mods; 

        foreach (var kvp in mods)
        {
            var id = kvp.Key;
            var modEntry = kvp.Value;

            string name = modEntry.Name;
            string version = modEntry.Version;
            string iconFilename = modEntry.IconFilename;
            Uri iconPath = new Uri(Path.Combine(EnvironmentManager.GetDataDirectory(), "icons", iconFilename));

            AllMods.Add(new ModCardViewModel(id, name, $"v{version}", iconPath));
        }

    }
}

public partial class LibraryPage : UserControl
{

    public LibraryPage()
    {
        var balls = new LibraryPageViewModel();
        DataContext = balls;

        InitializeComponent();
    }

    private async void OnAddModClicked(object? sender, RoutedEventArgs e)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return;

        var folders = await topLevel.StorageProvider.OpenFolderPickerAsync(new Avalonia.Platform.Storage.FolderPickerOpenOptions
        {
            Title = "Select a mod directory containing a valid Ren'Py game structure",
            AllowMultiple = false
        });

        if (folders.Count > 0)
        {
            string modDirectory = folders[0].Path.LocalPath;
            await ModIndexer.ProcessAndAddNewModAsync(modDirectory);
        }
    }

    private void OnModCardClicked(object? sender, RoutedEventArgs e)
    {
        if (sender is not FASettingsExpander button || button.DataContext is not ModCardViewModel mod) return;

        var t = new FASlideNavigationTransitionInfo();
        t.Effect = FASlideNavigationTransitionEffect.FromRight;
        // Navigate to the ModInterfacePage with the selected mod's ID
        MainView.instance?.ContentFrame.Navigate(typeof(ModInterfacePage), mod.Id, t);
    }
}
