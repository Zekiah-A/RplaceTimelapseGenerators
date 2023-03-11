using System.IO;
using System.Threading.Tasks;
using SkiaSharp;

namespace TimelapseGen;

internal static class DateGenerator
{
    public static async Task GenerateImageFrom(string date, string dateDirectory, string fileName)
    {
        var surface = SKSurface.Create(new SKImageInfo(1200, 128, SKColorType.Bgra8888, SKAlphaType.Premul));
        var paint = new SKPaint { IsAntialias = true, Color = SKColors.White, TextSize = 96 };
        var paint2 = new SKPaint { IsAntialias = true, Color = SKColors.Black, TextSize = 96, FakeBoldText = true};

        surface.Canvas.DrawText(date, 6, 102, paint2);
        surface.Canvas.DrawText(date, 8, 104, paint);
        surface.Canvas.Flush();
        
        await using var stream = File.Create(Path.Combine(dateDirectory, fileName + ".png"));
        stream.Seek(0, SeekOrigin.Begin);
        var imageData = surface.Snapshot().Encode(SKEncodedImageFormat.Png, 70);
        imageData.SaveTo(stream);
        await stream.FlushAsync();
        stream.Close();
    }
}