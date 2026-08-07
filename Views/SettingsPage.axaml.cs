using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using FluentAvalonia.UI.Controls;
using System.Diagnostics;

namespace VirtualClub.Views
{
    public partial class SettingsPage : UserControl
    {
        public static readonly StyledProperty<string> BaseDirectoryProperty =
        AvaloniaProperty.Register<SettingsPage, string>(nameof(BaseDirectory));
        public string BaseDirectory
        {
            get => App.AppDataService.Settings.BaseGameInstallationPath;
            set => App.AppDataService.Settings.BaseGameInstallationPath = value;
        } 

        public SettingsPage()
        {
            InitializeComponent();
        }

        private void OnThemeSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            // why tf can this be null
            if (ThemeComboBox is null) return;
            if (ThemeComboBox.SelectedItem is ComboBoxItem item)
            {
                Debug.WriteLine($"Selected theme: {item.Content}");
                switch (item.Content.ToString())
                {
                    case "Light":
                        App.AppDataService.Settings.Theme = Core.Theme.Light;
                        break;
                    case "Dark":
                        App.AppDataService.Settings.Theme = Core.Theme.Dark;
                        break;
                    default:
                        App.AppDataService.Settings.Theme = Core.Theme.Default;
                        break;
                }

                App.AppDataService.Save();

                Application.Current.RequestedThemeVariant = App.AppDataService.Settings.Theme switch
                {
                    Core.Theme.Light => Avalonia.Styling.ThemeVariant.Light,
                    Core.Theme.Dark => Avalonia.Styling.ThemeVariant.Dark,
                    _ => Avalonia.Styling.ThemeVariant.Default
                };
            }
        }

        private void OnCheckForUpdatesClicked(object? sender, RoutedEventArgs e)
        {
            Debug.WriteLine("Checking for updates...");
        }

        private async void OnBrowseBaseDirectoryClicked(object? sender, RoutedEventArgs e)
        {
            // Open a folder dialog to select the base game install directory
            var topLevel = TopLevel.GetTopLevel(this);
            if (topLevel == null) return;

            var folders = await topLevel.StorageProvider.OpenFolderPickerAsync(new Avalonia.Platform.Storage.FolderPickerOpenOptions
            {
                Title = "Select a mod directory containing a valid Ren'Py game structure",
                AllowMultiple = false
            });

            Debug.WriteLine($"Selected folders: {folders.Count}");

            if (folders.Count > 0)
            {
                string selectedPath = folders[0].Path.LocalPath;
                Debug.WriteLine($"Selected base directory: {selectedPath}");
                BaseDirectory = selectedPath;
                App.AppDataService.Save();
            }
        }
    }
}