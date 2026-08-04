///
/// <Border Background="#E0E0E0" CornerRadius="16" Padding="12,6" HorizontalAlignment="Left">
//     <StackPanel Orientation="Horizontal" Spacing="6">
//         <TextBlock Text="Avalonia" VerticalAlignment="Center" />
//         <Button Content="✕" Padding="0" Background="Transparent" BorderThickness="0"/>
//     </StackPanel>
// </Border>


using Avalonia;
using Avalonia.Controls;

namespace VirtualClub.Controls;

public class Chip : ContentControl
{
    public static readonly StyledProperty<bool> IsSelectedProperty =
        AvaloniaProperty.Register<Chip, bool>(nameof(IsSelected));

    public bool IsSelected
    {
        get => GetValue(IsSelectedProperty);
        set => SetValue(IsSelectedProperty, value);
    }
}