namespace NativeTimelapseGenerator.Console;

public static class Extensions
{
    public static string[] SplitChunks(this string input, int chunkSize)
    {
        var chunkCount = (input.Length + chunkSize - 1) / chunkSize;
        var result = new string[chunkCount];

        for (var i = 0; i < chunkCount; i++)
        {
            var start = i * chunkSize;
            var length = Math.Min(chunkSize, input.Length - start);
            result[i] = input.Substring(start, length);
        }

        return result;
    }
}