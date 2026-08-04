using System;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
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

/// <summary>
/// Takes in a task, and returns a [Task] that completes after a minimum of [delay] milliseconds. Can be canceled by [cancellationToken].
/// </summary>
public static class TaskExtensions
{
    public static async Task WithMinimumDelay(this Task task, int delay, CancellationToken cancellationToken = default)
    {
        var delayTask = Task.Delay(delay, cancellationToken);
        await Task.WhenAll(task, delayTask);
    }
}
