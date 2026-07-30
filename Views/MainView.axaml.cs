using System;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using FluentAvalonia.UI.Controls;

namespace VirtualClub.Views;

public partial class MainView : UserControl
{
    // literally dont like doing this at all
    public static MainView? instance {get; private set;}
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
    }


    private void OnNavViewSelectionChanged(Object? sender, FANavigationViewSelectionChangedEventArgs args)
    {
        if (args.SelectedItem is not FANavigationViewItem item)
            return;

        if (args.SelectedItem == NavView.SettingsItem)
        {
            ContentFrame.Navigate(typeof(SettingsView));
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
}