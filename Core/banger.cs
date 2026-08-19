using System;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;

using System;
using System.IO;
using System.Runtime.InteropServices;
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

    /// <summary>
    /// Creates a generic desktop shortcut to execute any shell command.
    /// </summary>
    /// <param name="shortcutName">Name of the shortcut file (without extension).</param>
    /// <param name="command">The CLI command or executable to run.</param>
    /// <param name="args">Arguments passed to the command.</param>
    /// <param name="workingDir">Optional working directory.</param>
    /// <param name="iconPath">Path to icon (.ico on Win, .png/.svg on Linux, .icns on Mac).</param>
    public static void CreateShortcut(string shortcutName, string command, string args = "", string workingDir = "", string iconPath = "")
    {
        string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
        if (string.IsNullOrEmpty(workingDir)) workingDir = AppContext.BaseDirectory;

        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            CreateWindowsShortcut(desktopPath, shortcutName, command, args, workingDir, iconPath);
        }
        else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            CreateLinuxShortcut(desktopPath, shortcutName, command, args, workingDir, iconPath);
        }
        else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
        {
            CreateMacShortcut(desktopPath, shortcutName, command, args, workingDir, iconPath);
        }
    }

    #region Windows (.lnk via WScript COM)

    private static void CreateWindowsShortcut(string desktopPath, string name, string command, string args, string workingDir, string iconPath)
    {
        string shortcutPath = Path.Combine(desktopPath, $"{name}.lnk");
        Type? shellType = Type.GetTypeFromProgID("WScript.Shell");

        if (shellType == null)
            throw new InvalidOperationException("WScript.Shell COM Object unavailable.");

        dynamic shell = Activator.CreateInstance(shellType)!;
        dynamic shortcut = shell.CreateShortcut(shortcutPath);

        shortcut.TargetPath = command;
        shortcut.Arguments = args;
        shortcut.WorkingDirectory = workingDir;

        if (!string.IsNullOrEmpty(iconPath) && File.Exists(iconPath))
        {
            shortcut.IconLocation = iconPath;
        }

        shortcut.Save();
    }

    #endregion

    #region Linux (freedesktop.org .desktop)

    private static void CreateLinuxShortcut(string desktopPath, string name, string command, string args, string workingDir, string iconPath)
    {
        string shortcutPath = Path.Combine(desktopPath, $"{name}.desktop");
        string execLine = string.IsNullOrWhiteSpace(args) ? command : $"{command} {args}";

        string desktopFileContent = $"""
        [Desktop Entry]
        Type=Application
        Version=1.0
        Name={name}
        Exec={execLine}
        Path={workingDir}
        Terminal=false
        StartupNotify=true
        """;

        if (!string.IsNullOrEmpty(iconPath) && File.Exists(iconPath))
        {
            desktopFileContent += $"\nIcon={iconPath}";
        }

        File.WriteAllText(shortcutPath, desktopFileContent);

        // Make executable (chmod +x)
        MakeExecutableLinux(shortcutPath);
    }

    private static void MakeExecutableLinux(string filePath)
    {
        // 0755 permissions using native POSIX call
        chmod(filePath, 0x1ED);
    }

    // ?
    [DllImport("libc", SetLastError = true)]
    private static extern int chmod(string pathname, uint mode);

    #endregion

    #region macOS (.command executable script)

    /// TODO: this part is vibed. i do not own a mac so im not sure if this works
    private static void CreateMacShortcut(string desktopPath, string name, string command, string args, string workingDir, string iconPath)
    {
        string shortcutPath = Path.Combine(desktopPath, $"{name}.command");
        string execLine = string.IsNullOrWhiteSpace(args) ? command : $"{command} {args}";

        // macOS executable script targeting zsh
        string scriptContent = $"""
        #!/usr/bin/zsh
        cd "{workingDir}"
        {execLine}
        """;

        File.WriteAllText(shortcutPath, scriptContent);

        // Make executable (chmod +x)
        chmod(shortcutPath, 0x1ED);

        // Uses AppleScript to set the icon of the file in Finder
        string appleScript = $"""
        use framework "Foundation"
        use framework "AppKit"

        set filePath to "{shortcutPath}"
        set iconPath to "{iconPath}"

        set theImage to current application's NSImage's alloc()'s initWithContentsOfFile:iconPath
        current application's NSWorkspace's sharedWorkspace()'s setIcon:theImage forFile:filePath options:0
        """;

        var psi = new System.Diagnostics.ProcessStartInfo("osascript", $"-e '{appleScript}'")
        {
            RedirectStandardOutput = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        System.Diagnostics.Process.Start(psi)?.WaitForExit();
    }

    #endregion
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