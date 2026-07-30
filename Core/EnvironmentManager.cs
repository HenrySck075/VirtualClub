using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace VirtualClub.Core;

public static class EnvironmentManager
{
    public static string GetDataDirectory()
    {
        // Maps cleanly to ~/.config/VirtualClub on Linux/Arch and AppData on Windows
        string baseData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        string appFolder = Path.Combine(baseData, "VirtualClub");
        
        if (!Directory.Exists(appFolder))
            Directory.CreateDirectory(appFolder);
            
        return appFolder;
    }

    public static void OpenFolder(string path)
    {
        var absPath = Path.GetFullPath(path);
        if (!Directory.Exists(absPath)) throw new DirectoryNotFoundException(absPath);

        ProcessStartInfo psi = new ProcessStartInfo
        {
            FileName = GetPlatformFileExplorer(),
            Arguments = RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? $"\"{absPath}\"" : absPath,
            UseShellExecute = true
        };

        Process.Start(psi);
    }

    private static string GetPlatformFileExplorer()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            return "explorer.exe";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            return "open";
        
        // Reliable fallback for automated desktop environments
        return "xdg-open";
    }

    public static string GetSaveDirectory(string modId)
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            return Path.Combine(Environment.GetEnvironmentVariable("APPDATA") ?? "", "RenPy", modId);
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library", "Application Support", "RenPy", modId);
            
        // Native Unix-like fallback
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".renpy", modId);
    }
}