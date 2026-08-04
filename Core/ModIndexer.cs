using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using Python.Runtime;
using VirtualClub;
using VirtualClub.Core;

public sealed class ModIndexer
{
    private static readonly Dictionary<string, Bitmap> _iconCache = new();
    public static Bitmap GetIcon(string filename) 
    {
        if (_iconCache.TryGetValue(filename, out Bitmap cachedIcon))
        {
            return cachedIcon;
        }

        string iconsDir = Path.Combine(App.AppDataService.AppDataFolder, "icons");
        string iconPath = Path.Combine(iconsDir, filename);

        Bitmap bitmap;
        if (File.Exists(iconPath))
        {
            bitmap = new Bitmap(iconPath);
        }
        else
        {
            // Return a default icon if the specified icon file doesn't exist
            bitmap = new Bitmap(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "logoplacehold.jpg"));
        }
        _iconCache[filename] = bitmap;
        return bitmap;
    }
    public static void DeleteMod(string modId)
    {
        var appDataService = App.AppDataService;
        if (appDataService.Index.Mods.ContainsKey(modId))
        {
            appDataService.Index.Mods.Remove(modId);
            appDataService.Save();

            // also deletes the mod's icon file
            string iconsDir = Path.Combine(appDataService.AppDataFolder, "icons");

            string iconPath = Path.Combine(iconsDir, $"{modId}.png");
            if (File.Exists(iconPath))
            {
                File.Delete(iconPath);
            }

            string windowIconPath = Path.Combine(iconsDir, $"{modId}.icon.png");
            if (File.Exists(windowIconPath))
            {
                File.Delete(windowIconPath);
            }

        }
    }
    public static async Task ProcessAndAddNewModAsync(string modDirectory)
    {
        // Ensure the directory exists
        if (!Directory.Exists(modDirectory))
        {
            throw new DirectoryNotFoundException($"The specified mod directory does not exist: {modDirectory}");
        }

        // Process the mod directory to extract metadata
        var modEntry = await ExtractModMetadataAsync(modDirectory);

        // Add the mod entry to the AppDataService
        var appDataService = App.AppDataService;
        appDataService.Index.Mods[modEntry.Id] = modEntry;

        // Save the updated mods index to disk
        appDataService.Save();
    }

    private static async Task<ModEntry> ExtractModMetadataAsync(string modDirectory)
    {
        var modGameDir = Path.Combine(modDirectory, "game");
        var baseGameDir = Path.Combine(App.AppDataService.Settings.BaseGameInstallationPath, "game");
        var indexes = new Dictionary<string, PyDict>(); // Placeholder for RPA index data

        await RenpyArchiveTools.InitializePythonAsync();

        var rpaReaderModule = RenpyArchiveTools.rpaReaderModule!;
        var rpycReaderModule = RenpyArchiveTools.rpycReaderModule!;

        foreach (var root in new[] { modGameDir, baseGameDir })
        {
            foreach (var file in Directory.EnumerateFiles(root, "*.rpa", SearchOption.AllDirectories))
            {
                var index = rpaReaderModule.InvokeMethod("read_rpa_index", new PyString(file));
                indexes[file] = index.As<PyDict>();
            }
        }

        byte[]? readGameFile(string filepath)
        {
            var modFilePath = Path.Combine(modGameDir, filepath);
            if (File.Exists(modFilePath))
            {
                return File.ReadAllBytes(modFilePath);
            }
            else
            {
                Debug.WriteLine($"Searching for {filepath} in RPA archives...");
                foreach (var kvp in indexes)
                {
                    var archiveFile = kvp.Key;
                    var index = kvp.Value;

                    Debug.WriteLine($"Checking archive: {archiveFile}");
                    Debug.WriteLine("Available files in archive:");
                    foreach (var key in index.Keys())
                    {
                        Debug.WriteLine($"- {key.As<string>()}");
                    }

                    if (index.HasKey(filepath))
                    {
                        return rpaReaderModule.InvokeMethod("extract_single_file", new PyString(archiveFile), new PyString(filepath), index).As<byte[]>();
                    }
                }
            }
            return null;
        };

        var statements = rpycReaderModule.InvokeMethod("get_rpyc_statements", rpycReaderModule.InvokeMethod("peek_rpyc", readGameFile("options.rpyc").ToPython())).As<PyList>();

        var requested_defines = new Dictionary<string, List<string>>
        {
            { "config", new List<string> { "name", "window_icon", "version", "save_directory" } },
            { "build", new List<string> { "name" } }
        };


        var pyDefines = rpycReaderModule.InvokeMethod("lookup_defines", statements, requested_defines.ToPython()).As<PyDict>();

        var defines = new Dictionary<string, string>();
        foreach (PyObject key in pyDefines.Keys())
        {
            using PyObject val = pyDefines[key];
            defines[key.As<string>()] = val.As<string>();
        }

        string removePrefix(string str, string prefix)
        {
            if (str.StartsWith(prefix))
            {
                return str.Substring(prefix.Length);
            }
            return str;
        }

        string name = defines.ContainsKey("config.name") ? defines["config.name"] : "Doki Doki Modding Club!";
        string version = defines.ContainsKey("config.version") ? defines["config.version"] : "1.0.0";
        string icon = defines.ContainsKey("config.window_icon") ? removePrefix(defines["config.window_icon"], "/") : "";
        string buildId = defines.ContainsKey("build.name") ? defines["build.name"] : "DDLC";
        string saveDirectory = defines.ContainsKey("config.save_directory") ? defines["config.save_directory"] : "DDLC";

        string modId = Guid.NewGuid().ToString(); // Using a new GUID for the mod ID
        
        if (string.IsNullOrEmpty(icon))
        {
            icon = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "logoplacehold.jpg");
        }

        /// TODO: somehow move this logic out of the extraction function
        Debug.WriteLine($"Scanning for icon at: {icon}");

        byte[]? iconContent = readGameFile(icon);

        string iconFilename = $"{modId}.png";

        // Save the icon to the icons directory
        if (iconContent != null)
        {
            string iconsDir = Path.Combine(App.AppDataService.AppDataFolder, "icons");
            Directory.CreateDirectory(iconsDir);
            new Bitmap(
                new MemoryStream(iconContent)).Save(Path.Combine(iconsDir, iconFilename), 
                new PngBitmapEncoderOptions() // shut up
            );
            
            // icon for the window title bar
            LarpingUtils.Iconize(Path.Combine(iconsDir, iconFilename), Path.Combine(iconsDir, $"{modId}.icon.png"), 180, 180);
        }

        return new ModEntry
        {
            Id = modId,
            Name = name,
            Version = version,
            BuildId = buildId,
            Directory = modDirectory,
            IconFilename = iconFilename
        };
    }
}