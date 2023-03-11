// Fullest canvas backup generator, can generate the backups, dates and dates animation for
// both canvas 1 and canvas 2. However, requires the most work to set up (is used for special
// cases, such as for full canvas timelapse youtube videos)
using System.Globalization;
using System.Net;
using System.Runtime.InteropServices;
using TimelapseGen;

var client = new HttpClient();
var c1dir = Path.Combine(Directory.GetCurrentDirectory(), "Canvas1");
var c2dir = Path.Combine(Directory.GetCurrentDirectory(), "Canvas2");
var datesC1dir = Path.Combine(Directory.GetCurrentDirectory(), "DatesCanvas1");
var datesC2dir = Path.Combine(Directory.GetCurrentDirectory(), "DatesCanvas2");

Directory.CreateDirectory(c1dir);
Directory.CreateDirectory(c2dir);

// Via HTTP, get every backup from the official canvas 1 GIT, making use of the git history list, fetch, generate,
// and save it into the canvas 1 directory, with a name corresponding to it's order in the backups.
// Populate URLS with boards, from git history (git clone https://rslashplace2/rslashplace2.github.io,
// (then in the root rplace directory) git log --follow -- place > place_log.txt).
// Server/Canvas 1 special 500x500 pixel canvas special event occurred somewhere around May 14, 2022, and may be found
// slightly corrupted in those commits.
var canvas1CommitInfos = new List<CommitInfo>();
var canvas1Rendered = 0;

var logs = File.ReadAllLines(Path.Join(Directory.GetCurrentDirectory(), "canvas_1_log.txt"));
var index = 0;
while (index < logs.Length)
{
    var current = logs[index];
    var commit = current.Split().Last();
    var date = logs[index + 2].Split("Date:").Last().Split("+").First().Trim();
    index += 6;
    canvas1CommitInfos.Add(new CommitInfo(commit, date));
}

canvas1CommitInfos.Reverse();

Console.ForegroundColor = ConsoleColor.DarkCyan;
Console.WriteLine("Started rendering canvas 1 backups.");
Console.ResetColor();

await Parallel.ForEachAsync(canvas1CommitInfos, RenderCanvas1Commit);

async ValueTask RenderCanvas1Commit(CommitInfo commitInfo, CancellationToken cancellationToken)
{
    var response = await client.GetAsync(
            "https://github.com/rslashplace2/rslashplace2.github.io/blob/" + commitInfo.CommitHash + "/place?raw=true",
            HttpCompletionOption.ResponseHeadersRead);
    
    await TimelapseGenerator.GenerateImageFrom
    (
        await response.Content.ReadAsByteArrayAsync(),
        2000,
        2000,
        c1dir,
        commitInfo.CommitDate
    );
    
    canvas1Rendered += 1;
    Console.WriteLine("Rendered " + canvas1Rendered + " of canvas 1 backups " + commitInfo.CommitDate);
}

Console.ForegroundColor = ConsoleColor.Green;
Console.WriteLine("Finished rendering canvas 1 backups.");
Console.ResetColor();


// Render all frames from Canvas 2 rplace canvas, these canvases do not rely on git, and a full backup list existis on
// the canvas 2 server.
var canvas2BackupList = (await File.ReadAllLinesAsync("/run/media/zekiah/RSLASHPLACE/backuplist_todo.txt")).ToList();
var canvas2Rendered = 0;
var i = 0;

await Parallel.ForEachAsync(canvas2BackupList, RenderCanvas2Backup);

async ValueTask RenderCanvas2Backup(string backupName, CancellationToken cancellationToken)
{
    i++;
    if (i % 6 == 0)
    {
        return;
    }

    var backupPath = Path.Combine("/run/media/zekiah/RSLASHPLACE/PlaceHttpServer", backupName);
    if (string.IsNullOrEmpty(backupName) || !File.Exists(backupPath))
    {
        return;
    }

    var bytes = await File.ReadAllBytesAsync(backupPath);

    // 500x500 -> 250000, 500 * 750 -> 375000, 750 x 750 -> 562500
    await TimelapseGenerator.GenerateImageFrom
    (
        bytes,
        bytes.Length > 375000 ? 750 : 500,
        bytes.Length > 250000 ? 750 : 500,
        c2dir,
        backupName
    );

    canvas2Rendered += 1;
    Console.WriteLine("Rendered " + canvas2Rendered + " of canvas 2 backups " + backupName);
}

Console.ForegroundColor = ConsoleColor.Green;
Console.WriteLine("Finished rendering canvas 2 backups. Canvas rendering completed.");
Console.ResetColor();

// Rename files to correct date time format for canvas 1
foreach (var filePath in Directory.GetFiles(c1dir))
{
    // (from Friday April 8 00:36:13 2022) -> 2022-04-08 00:36:13
    var splitPath = filePath.Split("/");
    var parsed = DateTime.ParseExact(splitPath.Last(), "dddd MMMM d HH:mm:ss yyyy", NumberFormatInfo.InvariantInfo);
    var beheaded = splitPath.Take(splitPath.Length - 1).Aggregate((a, b) => a + "/" + b);
    Console.WriteLine(Path.Combine(beheaded, parsed.ToString("yyyy-MM-dd HH:mm:ss") + ".png"));
    File.Move(filePath, Path.Combine(beheaded, parsed.ToString("yyyy-MM-dd HH:mm:ss") + ".png"));
}

Console.ForegroundColor = ConsoleColor.Green;
Console.WriteLine("Finished parsing canvas 1 dates.");
Console.ResetColor();


// Rename files to correct date time format for canvas 2
foreach (var filePath in Directory.GetFiles(c2dir))
{
    // (from 01 08 2022 00:22:49) -> 2022-08-01 00:22:49
    var splitPath = filePath.Split("/");
    if (!DateTime.TryParseExact(splitPath.Last(), "dd MM yyyy HH:mm:ss",
        NumberFormatInfo.InvariantInfo, DateTimeStyles.None, out var parsed))
    {
        continue;
    }
    var beheaded = splitPath.Take(splitPath.Length - 1).Aggregate((a, b) => a + "/" + b);

    Console.WriteLine(Path.Combine(beheaded, parsed.ToString("yyyy-MM-dd HH:mm:ss")));
    File.Move(filePath, Path.Combine(beheaded, parsed.ToString("yyyy-MM-dd HH:mm:ss")));
}

Console.ForegroundColor = ConsoleColor.Green;
Console.WriteLine("Finished parsing canvas 2 dates.");
Console.ResetColor();


// Generate date animation 
foreach (var renderPath in Directory.GetFiles(c1dir).Select(renderPath =>
             DateTime.ParseExact(renderPath.Split("/").Last().Replace(".png", ""), "yyyy-MM-dd HH:mm:ss",
                 NumberFormatInfo.InvariantInfo, DateTimeStyles.None)).Order())
{
    await DateGenerator.GenerateImageFrom(renderPath.ToString("ddd, dd MMM yyy HH:mm:ss"), datesC1dir, renderPath.ToString("yyyy-MM-dd HH_mm_ss"));
}

Console.WriteLine("Completed rendering canvas 1 dates");

foreach (var renderPath in Directory.GetFiles(c2dir).Select(renderPath =>
             DateTime.ParseExact(renderPath.Split("/").Last().Replace(".png", ""), "yyyy-MM-dd HH:mm:ss",
                 NumberFormatInfo.InvariantInfo, DateTimeStyles.None)).Order())
{
    await DateGenerator.GenerateImageFrom(renderPath.ToString("ddd, dd MMM yyy HH:mm:ss"), datesC2dir, renderPath.ToString("yyyy-MM-dd HH_mm_ss"));
}

Console.WriteLine("Completed rendering canvas 2 dates");

// Canvas 2: (after appending each file with PNG extension)
// ffmpeg -framerate 24 -pattern_type glob -i Canvas2/'*.png' -vcodec libx264  -b:v 5M -vf "pad=width=750:height=750:x=-1:y=-1:color=black",scale=2000:2000:flags=neighbor Canvas2.mp4
// Canvas 1:
// ffmpeg -framerate 24 -pattern_type glob -i Canvas1/'*.png' -vcodec libx264  -b:v 5M Canvas1.mp4
// for x in *; do     mv -- "$x" "${x//Sun/Sunday}"; done
// ffmpeg -framerate 24 -i "DatesCanvas1/%04d.png" DatesCanvas1.webm
