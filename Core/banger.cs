using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;

namespace VirtualClub.Core;

public sealed class LarpingUtils 
{
    public static void Iconize(string inputPath, string outputPath, int width = 256, int height = 256)
    {
        using (var image = new Bitmap(inputPath))
        {
            var resizedImage = image.CreateScaledBitmap(new Avalonia.PixelSize(width, height), BitmapInterpolationMode.HighQuality);
            resizedImage.Save(outputPath);
        }
    }
}