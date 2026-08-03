using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace VirtualClub.Core;

public enum Theme
{
    Light,
    Dark,
    Default // System theme
}

public sealed class AppSettings
{
    public string BaseGameInstallationPath { get; set; } = string.Empty;
    public Theme Theme { get; set; } = Theme.Default;
}

public sealed class ModEntry 
{
    public string Id { get; set; } = string.Empty;
    public string Name { get; set; } = "Unknown Mod";
    public string Version { get; set; } = "1.1.1";
    public string BuildId { get; set; } = "DDLC";
    public string Directory { get; set; } = string.Empty;
    public string IconFilename { get; set; } = string.Empty;

    public bool EnableDeveloperMode { get; set; } = false;
    public bool ForceRecompile { get; set; } = false;

    public string SaveDirectory
    {
        get
        {
            // scan renpy's global save directory for a folder that matches the mod's id

            // something like this in python for example
            /*
                def get_save_dir(self):        
        if platform.system() == "Windows":
            save_dir = os.path.join(os.environ.get("APPDATA", ""), "RenPy", self.modId)
        elif platform.system() == "Darwin":
            save_dir = os.path.join(os.path.expanduser("~"), "Library", "Application Support", "RenPy", self.modId) # TODO: i dont have a mac can sb check this
        else: # Linux and other Unix-like systems
            save_dir = os.path.join(os.path.expanduser("~"), ".renpy", self.modId)

        if not os.path.exists(save_dir):
            save_dir = os.path.join(self.folder, "game", "saves") # the fallback path

        return save_dir
            */

            string? saveDir;
            if (Environment.OSVersion.Platform == PlatformID.Win32NT)
            {
                saveDir = Path.Combine(Environment.GetEnvironmentVariable("APPDATA") ?? "", "RenPy", Id);
            }
            else if (Environment.OSVersion.Platform == PlatformID.MacOSX)
            {
                saveDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library", "Application Support", "RenPy", Id);
            }
            else // Linux and other Unix-like systems
            {
                saveDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".renpy", Id);
            }
            
            if (!System.IO.Directory.Exists(saveDir))
            {
                saveDir = Path.Combine(Directory, "game", "saves"); // the fallback path
            }

            return saveDir;
        }
    }
}

public sealed class ModIndex
{
    public Dictionary<string, ModEntry> Mods { get; set; } = new Dictionary<string, ModEntry>();
    public List<string> LastOpenedMods { get; set; } = new List<string>();
}

[JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase, WriteIndented = true)]
[JsonSerializable(typeof(AppSettings))]
[JsonSerializable(typeof(ModEntry))]
[JsonSerializable(typeof(ModIndex))]
public partial class AppdataJsonContext : JsonSerializerContext {}

public class AppDataService
{
    // for the primitives
    private static readonly JsonSerializerOptions JsonOptions = new JsonSerializerOptions
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public AppSettings Settings { get; private set; }
    public string SettingsFilePath { get; }
    // key is mod id, for lookup purpose obviously
    public ModIndex Index { get; private set; } = new ModIndex();
    public string ModsIndexFilePath { get; }

    public string AppDataFolder { get; private set; }

    public AppDataService()
    {
        // Platform-agnostic path: 
        // Windows: C:\Users\<User>\AppData\Roaming\<appName>
        // macOS: ~/.config/<appName> or ~/Library/Application Support/<appName>
        // Linux: ~/.config/<appName>
        string appDataFolder = EnvironmentManager.GetDataDirectory();

        AppDataFolder = appDataFolder;

        Directory.CreateDirectory(appDataFolder);
        SettingsFilePath = Path.Combine(appDataFolder, "settings.json");

        Settings = LoadSettings();

        ModsIndexFilePath = Path.Combine(appDataFolder, "mods.json");

        Index = LoadMods();
    }

    /// <summary>
    /// Loads settings from disk or returns default settings if missing/corrupted.
    /// </summary>
    public AppSettings LoadSettings()
    {
        try
        {
            if (File.Exists(SettingsFilePath))
            {
                string json = File.ReadAllText(SettingsFilePath);
                var loaded = JsonSerializer.Deserialize(json, AppdataJsonContext.Default.AppSettings);
                if (loaded != null) return loaded;
            }
        }
        catch (Exception ex)
        {
            // Log or handle error if needed; fallback to defaults
            System.Diagnostics.Debug.WriteLine($"Failed to load settings: {ex.Message}");
        }

        // Return defaults and save them to disk
        var defaults = new AppSettings();
        return defaults;
    }

    /// <summary>
    /// Loads mod entries from disk or returns an empty dictionary if missing/corrupted.
    /// </summary>
    public ModIndex LoadMods()
    {
        try
        {
            if (File.Exists(ModsIndexFilePath))
            {
                string json = File.ReadAllText(ModsIndexFilePath);
                var loaded = JsonSerializer.Deserialize(json, AppdataJsonContext.Default.ModIndex);
                if (loaded != null) return loaded;
            }
        }
        catch (Exception ex)
        {
            // Log or handle error if needed; fallback to empty dictionary
            System.Diagnostics.Debug.WriteLine($"Failed to load mods: {ex.Message}");
        }

        // Return empty dictionary and save it to disk
        var emptyMods = new ModIndex();
        return emptyMods;
    }

    /// <summary>
    /// Saves the current or provided settings object to disk.
    /// </summary>
    public void Save(AppSettings? settings = null, ModIndex? index = null)
    {
        if (settings != null)
        {
            Settings = settings;
        }

        if (index != null)
        {
            Index = index;
        }

        try
        {
            string json = JsonSerializer.Serialize(Settings, AppdataJsonContext.Default.AppSettings);
            File.WriteAllText(SettingsFilePath, json);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Failed to save settings: {ex.Message}");
        }

        try
        {
            string jsonMods = JsonSerializer.Serialize(Index, AppdataJsonContext.Default.ModIndex);
            File.WriteAllText(ModsIndexFilePath, jsonMods);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Failed to save mods: {ex.Message}");
        }
    }
}