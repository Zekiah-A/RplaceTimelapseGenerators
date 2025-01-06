using System.Runtime.InteropServices;

namespace NativeTimelapseGenerator.Console.Structs;

[StructLayout(LayoutKind.Explicit, Size = 32)]
public struct CanvasInfo
{
    [FieldOffset(0)]
    private IntPtr url;
    [FieldOffset(8)]
    private IntPtr commmitHash;
    [FieldOffset(16)]
    private IntPtr savePath;
    [FieldOffset(24)]
    public long Date;

    public string Url
    {
        get => Marshal.PtrToStringUTF8(url) ?? string.Empty;
        set => url = Marshal.StringToHGlobalAuto(value);
    }

    public string CommitHash
    {
        get => Marshal.PtrToStringUTF8(commmitHash) ?? string.Empty;
        set => commmitHash = Marshal.StringToHGlobalAuto(value);
    }

    public string SavePath
    {
        get => Marshal.PtrToStringUTF8(savePath) ?? string.Empty;
        set => savePath = Marshal.StringToHGlobalAuto(value);
    }
}