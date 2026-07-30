using System;
using System.Collections.ObjectModel;
using System.IO;
using Avalonia.Controls;
using Avalonia.Interactivity;
using FluentAvalonia.UI.Controls;
using VirtualClub.Core;

namespace VirtualClub.Views;

public record ModCardViewModel(string Id, string Name, string VersionText, Uri IconPath);

public sealed class LibraryPageViewModel
{
    public ObservableCollection<ModCardViewModel> AllMods { get; } = new();
    public ObservableCollection<ModCardViewModel> FilteredMods { get; } = new();


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

        FilterMods();
    }


    public void FilterMods(string? query = null)
    {
        FilteredMods.Clear();

        foreach (var mod in AllMods)
        {
            if (string.IsNullOrEmpty(query) || mod.Name.ToLowerInvariant().Contains(query))
            {
                FilteredMods.Add(mod);
            }
        }
    }
}

public partial class LibraryPage : UserControl
{

    public LibraryPage()
    {
        InitializeComponent();
        var balls = new LibraryPageViewModel();
        DataContext = balls;

        balls.LoadMods();
    }


    private void OnSearchTextChanged(object? sender, TextChangedEventArgs e)
    {
        if (DataContext is LibraryPageViewModel viewModel)
        {
            viewModel.FilterMods(SearchBox.Text?.Trim().ToLowerInvariant());
        }
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
            if (DataContext is LibraryPageViewModel viewModel)
            {
                viewModel.LoadMods();
            }
        }
    }

    private void OnModCardClicked(object? sender, RoutedEventArgs e)
    {
        if (sender is not FASettingsExpander button || button.DataContext is not ModCardViewModel mod) return;

        // Navigate to the ModInterfacePage with the selected mod's ID
        MainView.instance?.ContentFrame.Navigate(typeof(ModInterfacePage), mod.Id);
    }
}
