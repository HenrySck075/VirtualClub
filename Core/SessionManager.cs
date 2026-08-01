using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace VirtualClub.Core;

public class SessionManager
{
    public static async Task LaunchAsync(
        string modId, 
        string? selectedSaveId = null)
    {
        // delegate the task to `vclubmgr` binary sitting at the application root
        string vclubmgrPath = Path.Combine(AppContext.BaseDirectory, "vclubmgr");
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            vclubmgrPath += ".exe";
        
        if (!File.Exists(vclubmgrPath))
            throw new FileNotFoundException($"Could not find vclubmgr binary at {vclubmgrPath}. Check if you have installed the program correctly and try again.");
        
        // usage: vclubmgr [--start] [--save_id=<save_id>] [--launcher_root=<launcher_root>] <mod_id>
        var args = new List<string>
        {
            "--start",
            !string.IsNullOrEmpty(selectedSaveId) ? $"--save_id={selectedSaveId}" : "",
            $"--launcher_root=\"{AppContext.BaseDirectory}\"",
            modId
        };

        var startInfo = new ProcessStartInfo
        {
            FileName = vclubmgrPath,
            Arguments = string.Join(" ", args),
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        using (var process = new Process { StartInfo = startInfo })
        {
            process.Start();

            // Optionally, read the output and error streams
            string output = await process.StandardOutput.ReadToEndAsync();
            string error = await process.StandardError.ReadToEndAsync();

            await process.WaitForExitAsync();

            if (process.ExitCode != 0)
            {
                throw new Exception($"vclubmgr exited with code {process.ExitCode}. Error: {error}");
            }
        }
    }
}