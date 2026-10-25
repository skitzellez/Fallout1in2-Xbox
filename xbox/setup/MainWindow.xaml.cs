// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
// Fallout 1in2 for Xbox Setup: finds the player's own Fallout and Fallout 2, downloads Fallout 1in2 (and the
// optional HQ music) from their GitHub releases, and builds <drive>:\Fallout on a USB drive the Xbox app reads:
//   Fallout\master.dat, critter.dat          (from Fallout 2)
//   Fallout\Fallout1in2\...                  (the mod, without its Windows-only files)
//   Fallout\Fallout1in2\data\...             (Fallout 1's files, unpacked from its master.dat by the mod's dat2.exe)
// Then gives Xbox apps (ALL APPLICATION PACKAGES) access to the folder, which they need to read an NTFS drive.
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;


namespace Fallout1in2Setup
{
    public partial class MainWindow : Window
    {
        const string Fo1in2Repo = "rotators/Fo1in2";
        const string HqMusicRepo = "BGforgeNet/Fallout2-HQ-music";
        const long NeededBytes = 1900L << 20;
        const long MusicBytes = 400L << 20;

        // Windows-only parts of the mod package; the Xbox app is the engine
        static readonly string[] PcOnlyFiles = {
            "fallout2-ce.exe", "fallout2.exe", "ddraw.dll", "d3dx9_31.dll", "d3dx9_43.dll", "f2_res.dll",
            "f2_res_config.exe", "f2_res_readme.rtf", "undat.exe", "undat.bat", "undat.sh", "undat_files.txt",
        };

        string fo1, fo2;
        static readonly string Cache = Path.Combine(Path.GetTempPath(), "Fallout1in2Setup");

        public MainWindow()
        {
            InitializeComponent();
            System.Net.ServicePointManager.SecurityProtocol |= System.Net.SecurityProtocolType.Tls12;
            Loaded += (s, e) =>
            {
                SetGame(1, FindGame(1));
                SetGame(2, FindGame(2));
                RefreshDrives(null, null);
                // --selftest <folder>: builds into <folder>\Fallout with every extra and exits (exit code 0 = worked);
                // the drive's top folder is left alone. For checking a build of the setup on a PC without a USB drive.
                var args = Environment.GetCommandLineArgs();
                if (args.Length >= 3 && args[1] == "--selftest") SelfTest(args[2]);
            };
        }

        // ---------- finding the games ----------

        static bool IsGame(int which, string folder)
        {
            if (string.IsNullOrEmpty(folder) || !Directory.Exists(folder)) return false;
            bool dats = File.Exists(Path.Combine(folder, "master.dat")) && File.Exists(Path.Combine(folder, "critter.dat"));
            bool fo2Exe = Directory.EnumerateFiles(folder, "fallout2*.exe").Any();
            return dats && (which == 2 ? fo2Exe : !fo2Exe);
        }

        // The folder itself, or a game folder one level down (Epic keeps Fallout 2 in "Fallout 2 English").
        static string GameIn(int which, string folder)
        {
            if (IsGame(which, folder)) return folder;
            try
            {
                if (Directory.Exists(folder))
                    foreach (var sub in Directory.GetDirectories(folder))
                        if (IsGame(which, sub)) return sub;
            }
            catch (Exception) { }
            return null;
        }

        static IEnumerable<string> SteamLibraries()
        {
            var roots = new List<string>();
            var steam = Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string;
            if (steam != null) roots.Add(steam.Replace('/', '\\'));
            roots.Add(@"C:\Program Files (x86)\Steam");
            foreach (var root in roots.Distinct().ToList())
            {
                var vdf = Path.Combine(root, "steamapps", "libraryfolders.vdf");
                if (!File.Exists(vdf)) continue;
                foreach (Match m in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"([^\"]+)\""))
                    roots.Add(m.Groups[1].Value.Replace(@"\\", @"\"));
            }
            return roots.Distinct(StringComparer.OrdinalIgnoreCase).Select(r => Path.Combine(r, "steamapps", "common"));
        }

        static IEnumerable<string> GogFolders()
        {
            foreach (var hive in new[] { @"SOFTWARE\WOW6432Node\GOG.com\Games", @"SOFTWARE\GOG.com\Games" })
            {
                using (var key = Registry.LocalMachine.OpenSubKey(hive))
                {
                    if (key == null) continue;
                    foreach (var id in key.GetSubKeyNames())
                        using (var game = key.OpenSubKey(id))
                            if (game?.GetValue("path") is string p) yield return p;
                }
            }
        }

        static string FindGame(int which)
        {
            var names = which == 1
                ? new[] { "Fallout", "Fallout 1", "Fallout1", "Fallout Classic" }
                : new[] { "Fallout 2", "Fallout2" };
            var places = new List<string>();
            foreach (var lib in SteamLibraries()) places.AddRange(names.Select(n => Path.Combine(lib, n)));
            places.AddRange(GogFolders());
            foreach (var root in new[] { @"C:\GOG Games", @"C:\Program Files (x86)\GOG Galaxy\Games", @"C:\Program Files\Epic Games",
                                         @"C:\Program Files (x86)\GOG.com", @"C:\Games" })
                places.AddRange(names.Select(n => Path.Combine(root, n)));
            foreach (var place in places)
            {
                var found = GameIn(which, place);
                if (found != null) return found;
            }
            return null;
        }

        void SetGame(int which, string folder)
        {
            var ok = folder != null;
            if (which == 1) fo1 = folder; else fo2 = folder;
            var mark = which == 1 ? Fo1Mark : Fo2Mark;
            var path = which == 1 ? Fo1Path : Fo2Path;
            mark.Text = ok ? "✔" : "✘";
            mark.Foreground = (System.Windows.Media.Brush)FindResource(ok ? "Green" : "Amber");
            path.Text = ok ? folder : "not found - press Change and pick the game's folder";
            path.Foreground = (System.Windows.Media.Brush)FindResource(ok ? "Dim" : "Amber");
        }

        void PickGame(int which)
        {
            using (var dialog = new System.Windows.Forms.FolderBrowserDialog { Description = "Pick the folder where " + (which == 1 ? "Fallout" : "Fallout 2") + " is installed (it has master.dat in it)" })
            {
                if (dialog.ShowDialog() != System.Windows.Forms.DialogResult.OK) return;
                var found = GameIn(which, dialog.SelectedPath);
                if (found == null)
                    System.Windows.MessageBox.Show(this, "That folder doesn't hold " + (which == 1 ? "Fallout" : "Fallout 2") +
                        " (master.dat and critter.dat were not found, or it is the other Fallout).", "Fallout 1in2 Setup");
                else
                    SetGame(which, found);
            }
        }

        void PickFo1(object sender, RoutedEventArgs e) => PickGame(1);
        void PickFo2(object sender, RoutedEventArgs e) => PickGame(2);

        // ---------- the USB drive ----------

        class DriveChoice
        {
            public DriveInfo Drive;
            public override string ToString()
            {
                string label = string.IsNullOrEmpty(Drive.VolumeLabel) ? "USB drive" : Drive.VolumeLabel;
                return string.Format("{0}   {1}   ({2}, {3:0.0} GB free)", Drive.Name.TrimEnd('\\'), label, Drive.DriveFormat, Drive.AvailableFreeSpace / 1e9);
            }
        }

        void RefreshDrives(object sender, RoutedEventArgs e)
        {
            var system = Path.GetPathRoot(Environment.SystemDirectory);
            var list = new List<DriveChoice>();
            foreach (var d in DriveInfo.GetDrives())
            {
                try
                {
                    if (d.IsReady && (d.DriveType == DriveType.Removable || d.DriveType == DriveType.Fixed) &&
                        !string.Equals(d.Name, system, StringComparison.OrdinalIgnoreCase))
                        list.Add(new DriveChoice { Drive = d });
                }
                catch (Exception) { }
            }
            // removable drives first: that is almost always the stick meant for the Xbox
            Drives.ItemsSource = list.OrderBy(c => c.Drive.DriveType == DriveType.Removable ? 0 : 1).ToList();
            Drives.SelectedIndex = list.Count > 0 ? 0 : -1;
            if (list.Count == 0) DriveNote.Text = "Plug in the USB drive you use on your Xbox, then press Refresh.";
        }

        void DriveChanged(object sender, SelectionChangedEventArgs e)
        {
            var c = Drives.SelectedItem as DriveChoice;
            if (c == null) return;
            if (!string.Equals(c.Drive.DriveFormat, "NTFS", StringComparison.OrdinalIgnoreCase))
            {
                DriveNote.Text = "This drive is " + c.Drive.DriveFormat + ". The Xbox app can only read NTFS drives: format it as NTFS " +
                                 "(in File Explorer, right-click the drive > Format > NTFS - this erases it), then press Refresh.";
                DriveNote.Foreground = (System.Windows.Media.Brush)FindResource("Amber");
            }
            else
            {
                DriveNote.Text = "The game goes into " + c.Drive.Name + "Fallout. Nothing else on the drive is touched.";
                DriveNote.Foreground = (System.Windows.Media.Brush)FindResource("Dim");
            }
        }

        // ---------- making the drive ----------

        void Say(string text, double percent = -1)
        {
            Dispatcher.Invoke(() =>
            {
                Status.Text = text;
                Bar.IsIndeterminate = percent < 0;
                if (percent >= 0) Bar.Value = percent;
            });
        }

        async void MakeDrive(object sender, RoutedEventArgs e)
        {
            var choice = Drives.SelectedItem as DriveChoice;
            string problem = null;
            if (fo1 == null) problem = "Fallout wasn't found. Press Change next to it and pick its folder.";
            else if (fo2 == null) problem = "Fallout 2 wasn't found. Press Change next to it and pick its folder.";
            else if (choice == null) problem = "Plug in your USB drive and press Refresh.";
            else if (!string.Equals(choice.Drive.DriveFormat, "NTFS", StringComparison.OrdinalIgnoreCase))
                problem = "The USB drive must be NTFS (see the note under the drive).";
            else if (choice.Drive.AvailableFreeSpace < NeededBytes + (HqMusic.IsChecked == true ? MusicBytes : 0))
                problem = "Not enough room on the drive: it needs about " + ((NeededBytes + MusicBytes) >> 30) + " GB free.";
            if (problem != null)
            {
                Status.Text = problem;
                Status.Foreground = (System.Windows.Media.Brush)FindResource("Amber");
                return;
            }

            Status.Foreground = (System.Windows.Media.Brush)FindResource("Green");
            Go.IsEnabled = false;
            Bar.Visibility = Visibility.Visible;
            var options = new Options
            {
                Fo1 = fo1, Fo2 = fo2, Root = Path.Combine(choice.Drive.RootDirectory.FullName, "Fallout"),
                Music = HqMusic.IsChecked == true, Details = Details.IsChecked == true, Robodog = Robodog.IsChecked == true,
            };
            try
            {
                await Task.Run(() => Build(options));
                Say("ALL DONE!  Plug the drive into your Xbox and start Fallout 1in2.", 100);
            }
            catch (Exception ex)
            {
                Dispatcher.Invoke(() => Status.Foreground = (System.Windows.Media.Brush)FindResource("Amber"));
                Say("Setup stopped: " + ex.Message, 0);
            }
            Go.IsEnabled = true;
        }

        class Options
        {
            public string Fo1, Fo2, Root;
            public bool Music, Details, Robodog, SkipDriveRoot;
        }

        async void SelfTest(string folder)
        {
            int code = 1;
            try
            {
                await Task.Run(() => Build(new Options { Fo1 = fo1, Fo2 = fo2, Root = Path.Combine(folder, "Fallout"), Music = true, Details = true, SkipDriveRoot = true }));
                code = 0;
            }
            catch (Exception ex)
            {
                File.WriteAllText(Path.Combine(folder, "selftest-error.txt"), ex.ToString());
            }
            System.Windows.Application.Current.Shutdown(code);
        }

        static HttpClient Http()
        {
            var http = new HttpClient { Timeout = TimeSpan.FromMinutes(60) };
            http.DefaultRequestHeaders.UserAgent.ParseAdd("Fallout1in2XboxSetup/0.1");
            return http;
        }

        // The newest release's asset whose name matches, from the GitHub API.
        static (string url, long size, string tag) LatestAsset(HttpClient http, string repo, string namePattern)
        {
            var json = http.GetStringAsync("https://api.github.com/repos/" + repo + "/releases/latest").Result;
            var tag = Regex.Match(json, "\"tag_name\"\\s*:\\s*\"([^\"]+)\"").Groups[1].Value;
            // each asset: ... "size": N, ... "browser_download_url": "..." (the "uploader" object sits before "size",
            // so the file name is taken from the end of the link)
            foreach (Match m in Regex.Matches(json, "\"size\"\\s*:\\s*(\\d+)[^{}]*?\"browser_download_url\"\\s*:\\s*\"([^\"]+)\""))
            {
                var url = m.Groups[2].Value;
                if (Regex.IsMatch(Uri.UnescapeDataString(url.Substring(url.LastIndexOf('/') + 1)), namePattern, RegexOptions.IgnoreCase))
                    return (url, long.Parse(m.Groups[1].Value), tag);
            }
            throw new Exception("couldn't find the download on github.com/" + repo + " (check your internet connection)");
        }

        void Download(HttpClient http, string what, string url, long size, string file, double from, double to)
        {
            if (File.Exists(file) && new FileInfo(file).Length == size) return; // kept from an earlier run
            var part = file + ".part";
            using (var response = http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead).Result)
            {
                response.EnsureSuccessStatusCode();
                using (var input = response.Content.ReadAsStreamAsync().Result)
                using (var output = File.Create(part))
                {
                    var buffer = new byte[1 << 20];
                    long done = 0;
                    int n;
                    while ((n = input.Read(buffer, 0, buffer.Length)) > 0)
                    {
                        output.Write(buffer, 0, n);
                        done += n;
                        Say(string.Format("Downloading {0}...  {1} of {2} MB", what, done >> 20, size >> 20), from + (to - from) * done / Math.Max(1, size));
                    }
                }
            }
            if (new FileInfo(part).Length != size) throw new Exception(what + " download was incomplete, please try again");
            File.Delete(file);
            File.Move(part, file);
        }

        static void CopyIfChanged(string from, string to)
        {
            if (File.Exists(to) && new FileInfo(to).Length == new FileInfo(from).Length) return;
            File.Copy(from, to, true);
        }

        void Build(Options o)
        {
            Directory.CreateDirectory(Cache);
            using (var http = Http())
            {
                Say("Checking for the newest Fallout 1in2...");
                var mod = LatestAsset(http, Fo1in2Repo, @"^Fallout1in2\.zip$");
                var modZip = Path.Combine(Cache, "Fallout1in2-" + mod.tag + ".zip");
                Download(http, "Fallout 1in2 " + mod.tag, mod.url, mod.size, modZip, 0, 35);

                string musicZip = null;
                if (o.Music)
                {
                    var music = LatestAsset(http, HqMusicRepo, @"^fallout_music_hq_.*\.zip$");
                    musicZip = Path.Combine(Cache, "hqmusic-" + music.tag + ".zip");
                    Download(http, "HQ music " + music.tag, music.url, music.size, musicZip, 35, 60);
                }

                Directory.CreateDirectory(o.Root);
                Say("Copying Fallout 2's files...", 62);
                CopyIfChanged(Path.Combine(o.Fo2, "master.dat"), Path.Combine(o.Root, "master.dat"));
                CopyIfChanged(Path.Combine(o.Fo2, "critter.dat"), Path.Combine(o.Root, "critter.dat"));

                var game = Path.Combine(o.Root, "Fallout1in2");
                var tools = Path.Combine(Cache, "tools");
                Directory.CreateDirectory(tools);
                using (var zip = ZipFile.OpenRead(modZip))
                {
                    int i = 0;
                    foreach (var entry in zip.Entries)
                    {
                        i++;
                        if (entry.FullName.EndsWith("/")) continue;
                        var rel = entry.FullName.StartsWith("Fallout1in2/") ? entry.FullName.Substring(12) : entry.FullName;
                        var name = Path.GetFileName(rel).ToLowerInvariant();
                        // dat2.exe and the file list unpack Fallout 1 below; they stay on the PC
                        if (rel == "utils/dat2.exe" || name == "undat_files.txt")
                        {
                            entry.ExtractToFile(Path.Combine(tools, name), true);
                            continue;
                        }
                        if (rel.StartsWith("utils/") || (!rel.Contains("/") && PcOnlyFiles.Contains(name))) continue;
                        var target = Path.Combine(game, rel.Replace('/', '\\'));
                        // keep the player's own settings and saves when setup runs again over an older copy
                        if (File.Exists(target) && (name == "fallout2.cfg" || rel.StartsWith("data/SAVEGAME", StringComparison.OrdinalIgnoreCase))) continue;
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        if (!(File.Exists(target) && new FileInfo(target).Length == entry.Length)) entry.ExtractToFile(target, true);
                        if (i % 50 == 0) Say("Putting Fallout 1in2 on the drive...", 62 + 20.0 * i / zip.Entries.Count);
                    }
                }

                Say("Unpacking Fallout 1's files (this takes a minute)...", 83);
                var dat2 = new ProcessStartInfo(Path.Combine(tools, "dat2.exe"),
                    "x -d \"" + Path.Combine(game, "data") + "\" \"" + Path.Combine(o.Fo1, "master.dat") + "\" \"@" + Path.Combine(tools, "undat_files.txt") + "\"")
                { CreateNoWindow = true, UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true };
                using (var p = Process.Start(dat2))
                {
                    p.StandardOutput.ReadToEnd();
                    p.WaitForExit();
                    if (p.ExitCode != 0) throw new Exception("couldn't unpack Fallout 1's master.dat (is the Fallout folder complete?)");
                }

                if (musicZip != null)
                {
                    Say("Adding the HQ music...", 90);
                    var musicDir = Path.Combine(game, "data", "sound", "music");
                    Directory.CreateDirectory(musicDir);
                    using (var zip = ZipFile.OpenRead(musicZip))
                        foreach (var entry in zip.Entries.Where(en => en.Name.EndsWith(".acm", StringComparison.OrdinalIgnoreCase)))
                        {
                            var target = Path.Combine(musicDir, entry.Name);
                            if (!(File.Exists(target) && new FileInfo(target).Length == entry.Length)) entry.ExtractToFile(target, true);
                        }
                }

                Say("Choosing the extras...", 94);
                var extras = new List<string>();
                if (o.Details) extras.AddRange(new[] { "fo1_footprints", "fo1_alternative_forcefields", "fo1_v15_rework" });
                if (o.Robodog) extras.Add("fo1_robodog");
                SetMods(Path.Combine(game, "mods", "mods_order.txt"), extras,
                    new[] { "fo1_footprints", "fo1_alternative_forcefields", "fo1_v15_rework", "fo1_robodog" });

                Say("Letting the Xbox app read the folder...", 97);
                GrantXboxApps(o.Root, !o.SkipDriveRoot);
                File.WriteAllText(Path.Combine(game, "xbox_setup.txt"),
                    "Made by Fallout 1in2 for Xbox Setup " + DateTime.Now.ToString("yyyy-MM-dd HH:mm") + "\r\nFallout 1in2 " + mod.tag +
                    (musicZip != null ? "\r\nHQ music" : "") + "\r\nExtras: " + string.Join(", ", extras) + "\r\n");
            }
        }

        // mods_order.txt lists the mini-mods; a leading ';' turns one off.
        static void SetMods(string file, List<string> on, string[] managed)
        {
            if (!File.Exists(file)) return;
            var lines = File.ReadAllLines(file).ToList();
            foreach (var mod in managed)
            {
                int at = lines.FindIndex(l => l.TrimStart(';').Trim().Equals(mod, StringComparison.OrdinalIgnoreCase));
                var line = (on.Contains(mod) ? "" : ";") + mod;
                if (at >= 0) lines[at] = line; else lines.Add(line);
            }
            File.WriteAllLines(file, lines);
        }

        // Xbox apps run as "ALL APPLICATION PACKAGES" (S-1-15-2-1) and can only open NTFS folders that allow it.
        static void GrantXboxApps(string folder, bool driveRoot)
        {
            var root = Path.GetPathRoot(folder);
            RunIcacls("\"" + folder + "\" /grant *S-1-15-2-1:(OI)(CI)F /T /C /Q", false);
            if (!driveRoot) return;
            // the drive's top folder must let them in too; if it is locked, Windows asks once for permission
            if (!RunIcacls("\"" + root.TrimEnd('\\') + "\\\" /grant *S-1-15-2-1:(RX)", false))
                RunIcacls("\"" + root.TrimEnd('\\') + "\\\" /grant *S-1-15-2-1:(RX)", true);
        }

        static bool RunIcacls(string args, bool elevated)
        {
            var start = new ProcessStartInfo("icacls.exe", args) { CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden };
            if (elevated) { start.UseShellExecute = true; start.Verb = "runas"; }
            else start.UseShellExecute = false;
            try
            {
                using (var p = Process.Start(start))
                {
                    p.WaitForExit();
                    return p.ExitCode == 0;
                }
            }
            catch (Exception)
            {
                return false; // the permission prompt was declined
            }
        }
    }
}
