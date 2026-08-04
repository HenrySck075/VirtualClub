using System;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using FluentAvalonia.UI.Controls;
using FluentAvalonia.UI.Media.Animation;

namespace VirtualClub.Views;

public record SearchItem(string Name, Bitmap Icon, string Id);

public sealed class MainViewModel
{
    public ObservableCollection<SearchItem> Items { get; } = new();
    public void Populate()
    {
        Items.Clear();

        var mods = App.AppDataService.Index.Mods;

        foreach (var kvp in mods)
        {
            var id = kvp.Key;
            var modEntry = kvp.Value;

            Items.Add(new SearchItem(modEntry.Name, ModIndexer.GetIcon(modEntry.IconFilename), id));
        }
    }
}
public partial class MainView : UserControl
{
    // literally dont like doing this at all
    public static MainView? instance { get; private set; }
    public static readonly DirectProperty<MainView, bool> CanGoBackProperty =
            AvaloniaProperty.RegisterDirect<MainView, bool>(
                nameof(CanGoBack),
                o => o.CanGoBack);

    private bool _canGoBack;
    public bool CanGoBack
    {
        get => _canGoBack;
        private set => SetAndRaise(CanGoBackProperty, ref _canGoBack, value);
    }
    public MainView()
    {
        InitializeComponent();
        instance = this;

        NavView.SelectionChanged += OnNavViewSelectionChanged;

        this.AttachedToVisualTree += (s, e) =>
        {
            // 1. Select the item you want by default
            NavView.SelectedItem = NavView.MenuItems.OfType<FANavigationViewItem>().First();

            // 2. Force the content to load
            ContentFrame.Navigate(typeof(HomePage));
        };

        ContentFrame.Navigated += (sender, e) => UpdateCanGoBack();

        var seven = new MainViewModel();
        DataContext = seven;
        seven.Populate();
    }


    /// <summary>
    /// Current search syntaxes: (case insensitive besides maybe id and the prefixes)
    /// - no prefixes: search by mod names 
    /// - `id:{modId}`: search by mod id
    /// </summary>
    /// <returns>you</returns>
    private bool SearchFilter(string? search, object? i)
    {
        if (i is SearchItem item)
        {
            if (string.IsNullOrWhiteSpace(search))
                return true;

            if (search.StartsWith("id:", StringComparison.OrdinalIgnoreCase))
            {
                var idSearch = search.Substring(3);
                return item.Id == idSearch;
            }

            return item.Name.Contains(search, StringComparison.OrdinalIgnoreCase);
        }
        return false;
    }

    private void UpdateCanGoBack()
    {
        var contentType = ContentFrame.Content?.GetType();

        CanGoBack = contentType != typeof(HomePage)
                && contentType != typeof(LibraryPage)
                && contentType != typeof(SettingsPage);
    }

    private void OnNavViewSelectionChanged(Object? sender, FANavigationViewSelectionChangedEventArgs args)
    {
        if (args.SelectedItem is not FANavigationViewItem item)
            return;

        if (args.SelectedItem == NavView.SettingsItem)
        {
            ContentFrame.Navigate(typeof(SettingsPage));
            return;
        }

        switch (item.Tag?.ToString())
        {
            case "home":
                ContentFrame.Navigate(typeof(HomePage));
                break;
            case "list":
                ContentFrame.Navigate(typeof(LibraryPage));
                break;
        }
    }

    private void NavView_BackRequested(object? sender, FANavigationViewBackRequestedEventArgs e)
    {
        if (ContentFrame.CanGoBack)
        {
            var t = new FASlideNavigationTransitionInfo();
            t.Effect = FASlideNavigationTransitionEffect.FromLeft;
            ContentFrame.GoBack(t);
        }
    }

    private void ModSearchBox_SelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (sender is AutoCompleteBox acb && acb.SelectedItem is SearchItem selectedItem)
        {
            var t = new FASlideNavigationTransitionInfo();
            t.Effect = FASlideNavigationTransitionEffect.FromRight;
            ContentFrame.Navigate(typeof(ModInterfacePage), selectedItem.Id, t);
        }
    }
}