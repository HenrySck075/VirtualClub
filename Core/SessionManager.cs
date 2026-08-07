using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace VirtualClub.Core;

public class SessionManager
{
    // for all these dicts, the key is the mod id
    private static readonly Dictionary<string, (bool, Process)> _sessions = new Dictionary<string, (bool, Process)>();
    private static readonly Dictionary<string, string> _mountedDirectories = new Dictionary<string, string>();

    private static void _addSession(string modId, bool startedGame, Process process)
    {
        _sessions[modId] = (startedGame, process);
        process.EnableRaisingEvents = true;
        void Cleanup()
        {
            Debug.WriteLine($"Session for mod {modId} has exited.");
            process.Dispose();
            _sessions.Remove(modId);
            _mountedDirectories.Remove(modId);
        };

        if (process.HasExited)
        {
            Cleanup();
            return;
        }
        process.Exited += (sender, args) => Cleanup();
    }

    private static void _addLastOpenedMods(string modId)
    {
        var lastOpenedMods = App.AppDataService.Index.LastOpenedMods;
        if (lastOpenedMods.Contains(modId))
        {
            lastOpenedMods.Remove(modId);
        }
        lastOpenedMods.Insert(0, modId);
        // keep only the 10 most recent mods
        if (lastOpenedMods.Count > 10)
        {
            lastOpenedMods.RemoveRange(10, lastOpenedMods.Count - 10);
        }
    }

    // used on startup
    public static async Task RestoreSessionsList()
    {
        var mods = App.AppDataService.Index.Mods;

        var locksPath = Path.Combine(App.AppDataService.AppDataFolder, "locks");

        foreach (var key in mods.Keys)
        {
            // each lock files always contains 3 lines
            var lockFilePath = Path.Combine(locksPath, $"{key}.lock");
            if (File.Exists(lockFilePath))
            {
                try
                {
                    var lines = await File.ReadAllLinesAsync(lockFilePath);
                    if (lines.Length >= 3 && int.TryParse(lines[0], out int pid))
                    {
                        try
                        {
                            var process = Process.GetProcessById(pid);
                            if (!process.HasExited)
                            {
                                process.EnableRaisingEvents = true;
                                // the second line is the mounted directory path.
                                // in a perfectly normal case this would be a valid and read/writeable path so we dont need to do any checks
                                // but if it wasnt, uh, idk, blame somebody for the corrupt .lock files
                                if (lines.Length < 3)
                                {
                                    throw new Exception($"Lock file for mod ID {key} is corrupt.");
                                }
                                if (!Directory.Exists(lines[1]))
                                {
                                    throw new Exception($"Mounted directory for mod ID {key} does not exist.");
                                }
                                _mountedDirectories[key] = lines[1];
                                var startedGame = !(lines[2] == "0");
                                _addSession(key, startedGame, process);
                            }
                        }
                        catch (ArgumentException)
                        {
                            // Process with the specified PID does not exist
                            // nuke the lock

                            File.Delete(lockFilePath);
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error reading lock file for mod {key}: {ex.Message}");
                }
            }
        }
    }

    public static async Task LaunchAsync(
        string modId, 
        bool start = true, // false if mounted solely to browse
        string? selectedSaveId = null)
    {
        // if a session exists but for opening folder, kill it
        // if it does and NOT for doing that, throw
        if (_sessions.TryGetValue(modId, out var tung))
        {
            var (started, _) = tung;
            if (!started)
            {
                TerminateSession(modId);
            }
            else
            {
                throw new InvalidOperationException($"A session for mod {modId} is already running.");
            }
        }

        // delegate the task to `vclubmgr` binary sitting at the application root
        string vclubmgrPath = Path.Combine(AppContext.BaseDirectory, "vclubmgr");
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            vclubmgrPath += ".exe";
        
        if (!File.Exists(vclubmgrPath))
            throw new FileNotFoundException($"Could not find vclubmgr executable at {vclubmgrPath}. Check if you have installed the program correctly and try again.");
        
        // usage: vclubmgr [--start] [--save_id=<save_id>] [--launcher_root=<launcher_root>] <mod_id>
        var startInfo = new ProcessStartInfo
        {
            FileName = vclubmgrPath,
            UseShellExecute = false,
            RedirectStandardOutput = false,
            RedirectStandardError = false,
            RedirectStandardInput = false,
            CreateNoWindow = true,
        };
        if (start) startInfo.ArgumentList.Add("--start");
        if (!string.IsNullOrEmpty(selectedSaveId))
        {
            startInfo.ArgumentList.Add($"--save_id={selectedSaveId}");
        }
        startInfo.ArgumentList.Add($"--launcher_root={AppContext.BaseDirectory}");
        startInfo.ArgumentList.Add(modId);

        var process = new Process { StartInfo = startInfo };
        process.Start();

        _addSession(modId, start, process);
        if (start) _addLastOpenedMods(modId);

        // (wait and) get the lock file to pull the mount directory
        // wait until 5s passed or the process exited
        var lockFilePath = Path.Combine(App.AppDataService.AppDataFolder, "locks", $"{modId}.lock");
        
        await Task.Run(async () =>
        {
            var timeout = TimeSpan.FromSeconds(5);
            var startTime = DateTime.Now;
            while (!File.Exists(lockFilePath) && !process.HasExited)
            {
                if (DateTime.Now - startTime > timeout)
                {
                    throw new TimeoutException($"Timeout waiting for lock file for mod {modId}.");
                }
                await Task.Delay(100);
            }
        });

        // read the lock file to get the mount directory
        var lines = await File.ReadAllLinesAsync(lockFilePath);
        if (lines.Length < 2)
        {
            throw new Exception($"Lock file for mod ID {modId} is corrupt."); 
        }
        if (!Directory.Exists(lines[1]))
        {
            throw new Exception($"Mounted directory for mod ID {modId} does not exist.");
        }
        _mountedDirectories[modId] = lines[1];

    }

    public static bool IsSessionRunning(string modId, bool alsoChecksGameStarted = true)
    {
        if (_sessions.TryGetValue(modId, out var tung))
        {
            var (started, process) = tung;
            return (alsoChecksGameStarted ? started : true) && !process.HasExited;
        }
        return false;
    }

    public static void OnSessionExitCallback(string modId, Action callback)
    {
        if (_sessions.TryGetValue(modId, out var tung))
        {
            var (_, process) = tung;
            if (process.HasExited)
            {
                callback();
                return;
            }
            process.Exited += (sender, args) => callback();
        }
    }

    public static void TerminateSession(string modId)
    {
        if (_sessions.TryGetValue(modId, out var tung))
        {
            var (_, process) = tung;
            try
            {
                process.Kill(); // hope this sends sigterm
                process.WaitForExit();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error terminating session for mod {modId}: {ex.Message}");
            }
        }
    }

    public static async void OpenMountedDirectory(string id)
    {
        if (!IsSessionRunning(id, false)) await LaunchAsync(id, false);
        var folderPath = _mountedDirectories[id];
        if (folderPath != null && Directory.Exists(folderPath))
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = folderPath,
                    UseShellExecute = true
                });
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                Process.Start("open", folderPath);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                Process.Start("xdg-open", folderPath);
            }
        }

    }
}