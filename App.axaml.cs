using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using VirtualClub.Core;

namespace VirtualClub;

public partial class App : Application
{
    public static AppDataService AppDataService { get; private set; } = new AppDataService();

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = new MainWindow();
        }

        base.OnFrameworkInitializationCompleted();

        RequestedThemeVariant = AppDataService.Settings.Theme switch
        {
            Theme.Light => Avalonia.Styling.ThemeVariant.Light,
            Theme.Dark => Avalonia.Styling.ThemeVariant.Dark,
            _ => Avalonia.Styling.ThemeVariant.Default
        };
    }
}