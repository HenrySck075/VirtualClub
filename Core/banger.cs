using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;

namespace VirtualClub.Core;

public class UriToBitmapConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is Uri uri && uri.IsFile)
        {
            return new Bitmap(uri.LocalPath);
        }
        if (value is string path && !string.IsNullOrEmpty(path))
        {
            return new Bitmap(path);
        }
        return null;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotImplementedException();
}