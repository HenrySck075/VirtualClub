using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text.Json;
using Avalonia.Media.Imaging;

namespace VirtualClub.Core;

public record SaveGameEntry(string SaveId, string Name, DateTime DateModified, Bitmap Thumbnail);

public static class SaveManager
{
    public static List<SaveGameEntry> GetSaveFiles(string saveDir)
    {
        var results = new List<SaveGameEntry>();
        if (!Directory.Exists(saveDir)) return results;

        foreach (var savePath in Directory.EnumerateFiles(saveDir, "*-LT1.save"))
        {
            try
            {
                using var archive = ZipFile.OpenRead(savePath);
                
                // Read embedded thumbnail
                var imgEntry = archive.GetEntry("screenshot.png");
                Bitmap? bitmap = null;
                if (imgEntry != null)
                {
                    using var imgStream = imgEntry.Open();
                    bitmap = new Bitmap(imgStream);
                }

                // Read metadata json
                string saveName = Path.GetFileNameWithoutExtension(savePath).Replace("-LT1", "");
                var jsonEntry = archive.GetEntry("json");
                if (jsonEntry != null)
                {
                    using var reader = new StreamReader(jsonEntry.Open());
                    using var doc = JsonDocument.Parse(reader.ReadToEnd());
                    if (doc.RootElement.TryGetProperty("_save_name", out var nameProp) && !string.IsNullOrEmpty(nameProp.GetString()))
                    {
                        saveName = nameProp.GetString()!;
                    }
                }

                var fileInfo = new FileInfo(savePath);
                results.Add(new SaveGameEntry(
                    SaveId: Path.GetFileNameWithoutExtension(savePath).Replace("-LT1", ""),
                    Name: saveName,
                    DateModified: fileInfo.LastWriteTime,
                    Thumbnail: bitmap!
                ));
            }
            catch
            {
                // Ignore corrupt or locked saves
            }
        }

        return results;
    }
}