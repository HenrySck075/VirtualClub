using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using FluentAvalonia.Styling;
using VirtualClub.Core;
using VirtualClub.Views;

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
        else if (ApplicationLifetime is ISingleViewApplicationLifetime singleView)
        {
            singleView.MainView = new MainView();
        }

        base.OnFrameworkInitializationCompleted();

        RequestedThemeVariant = AppDataService.Settings.Theme switch
        {
            Theme.Light => Avalonia.Styling.ThemeVariant.Light,
            Theme.Dark => Avalonia.Styling.ThemeVariant.Dark,
            _ => Avalonia.Styling.ThemeVariant.Default
        };

        //var faTheme = AvaloniaLocator.CurrentMutable.GetService<FluentAvaloniaTheme>();
    }
}