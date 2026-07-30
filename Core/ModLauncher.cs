using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace VirtualClub.Core;

public class ModLauncher
{
    public static async Task LaunchAsync(
        string modId, 
        string mountDir, 
        string modDirectory, 
        string buildId, 
        bool forceRecompile, 
        bool devMode, 
        string? selectedSaveId = null)
    {
        string pythonExe = ResolvePythonExecutable(mountDir, modId);

        // Linux permission restoration fix
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            var mode = File.GetUnixFileMode(pythonExe);
            if (!mode.HasFlag(UnixFileMode.UserExecute))
            {
                File.SetUnixFileMode(pythonExe, mode | UnixFileMode.UserExecute | UnixFileMode.GroupExecute | UnixFileMode.OtherExecute);
            }
        }

        // Force recompile: purge loose .rpyc files
        if (forceRecompile)
        {
            foreach (var rpycFile in Directory.EnumerateFiles(mountDir, "*.rpyc", SearchOption.AllDirectories))
            {
                try { File.Delete(rpycFile); } catch { /* Ignore locked/permission files */ }
            }
        }

        // Bootstrapper resolution
        string bootstrapper = Path.Combine(mountDir, $"{buildId}.py");
        if (!File.Exists(bootstrapper))
            bootstrapper = Path.Combine(mountDir, "DDLC.py");

        var startInfo = new ProcessStartInfo
        {
            FileName = pythonExe,
            WorkingDirectory = mountDir,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        // Inject Environment Variables
        startInfo.EnvironmentVariables["MVC_MOD_ID"] = modId;
        if (devMode) startInfo.EnvironmentVariables["MVC_DEVELOPER"] = "Mon-ika";
        if (!string.IsNullOrEmpty(selectedSaveId)) startInfo.EnvironmentVariables["MVC_SAVE_ID"] = selectedSaveId;

        // Ren'Py 6 fallback flag
        if (!Directory.Exists(Path.Combine(modDirectory, "lib")))
        {
            startInfo.ArgumentList.Add("-EO");
        }
        startInfo.ArgumentList.Add(bootstrapper);

        // Linux LD_LIBRARY_PATH resolution
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            string pyDir = Path.GetDirectoryName(pythonExe)!;
            string currentLd = startInfo.EnvironmentVariables["LD_LIBRARY_PATH"] ?? "";
            startInfo.EnvironmentVariables["LD_LIBRARY_PATH"] = string.IsNullOrEmpty(currentLd) ? pyDir : $"{pyDir}:{currentLd}";
        }

        using var process = new Process { StartInfo = startInfo };
        
        process.OutputDataReceived += (s, e) => { if (e.Data != null) Console.WriteLine($"[OUT] {e.Data}"); };
        process.ErrorDataReceived += (s, e) => { if (e.Data != null) Console.Error.WriteLine($"[ERR] {e.Data}"); };

        process.Start();
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        await process.WaitForExitAsync();
    }

    private static string ResolvePythonExecutable(string mountDir, string modId)
    {
        string platformKey = GetPlatformKey();
        string exeName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "pythonw.exe" : "pythonw";

        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            return Path.Combine(mountDir, $"{modId}.app", "Contents", "MacOS", "pythonw");

        // Try Py3 -> Py2 -> direct platform dir
        string py3Path = Path.Combine(mountDir, "lib", $"py3-{platformKey}", exeName);
        if (File.Exists(py3Path)) return py3Path;

        string py2Path = Path.Combine(mountDir, "lib", $"py2-{platformKey}", exeName);
        if (File.Exists(py2Path)) return py2Path;

        string directPath = Path.Combine(mountDir, "lib", platformKey, exeName);
        if (File.Exists(directPath)) return directPath;

        throw new FileNotFoundException($"Could not find a valid Python executable for target environment: {platformKey}");
    }

    private static string GetPlatformKey()
    {
        bool is64 = RuntimeInformation.OSArchitecture == Architecture.X64;
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            return is64 ? "windows-x86_64" : "windows-i686";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            return is64 ? "linux-x86_64" : "linux-i686";

        return "unknown";
    }
}