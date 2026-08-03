using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using FluentAvalonia.UI.Windowing;

namespace VirtualClub;

public partial class MainWindow : FAAppWindow
{
    public MainWindow()
    {
        InitializeComponent();

        SplashScreen = new MainAppSplashScreen(this);

        TitleBar.ExtendsContentIntoTitleBar = true;
    }
}

public class MainAppSplashContent : UserControl
{
    public MainAppSplashContent()
    {
        InitializeComponent();
    }
    
    // do the most winui3 splash screen using the app icon
    public void InitializeComponent()
    {
        Content = new Image
        {
            Source = new Bitmap(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "app-icon.png")).CreateScaledBitmap(new PixelSize(100,100)),
            Stretch = Stretch.Uniform,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center
        };
    }
}

internal class MainAppSplashScreen : IFAApplicationSplashScreen
{
    public MainAppSplashScreen(MainWindow owner)
    {
        _owner = owner;
    }

    public string AppName { get; }
    public IImage AppIcon { get => new Bitmap(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "app-icon.png")); }
    public object SplashScreenContent => new MainAppSplashContent();
    public int MinimumShowTime => 2000;


    public Task RunTasks(CancellationToken cancellationToken)
    {
        // will arbitrarily wait for 0.5s for now, do fs operations have async versions?
        return Task.Delay(500, cancellationToken);
    }

    private MainWindow _owner;
}