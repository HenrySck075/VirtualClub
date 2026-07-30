using FluentAvalonia.UI.Windowing;

namespace VirtualClub;

public partial class MainWindow : FAAppWindow
{
    public MainWindow()
    {
        InitializeComponent();

        TitleBar.ExtendsContentIntoTitleBar = true;
    }
}