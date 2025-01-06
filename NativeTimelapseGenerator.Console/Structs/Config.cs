using System.Runtime.InteropServices;

namespace NativeTimelapseGenerator.Console.Structs;

[StructLayout(LayoutKind.Explicit, Size = 36)]
public struct Config
{
    [FieldOffset(0)]
	private IntPtr repoUrl;
    [FieldOffset(8)]
    private IntPtr downloadBaseUrl;
    [FieldOffset(16)]
    private IntPtr commitHashesFileName;
    [FieldOffset(24)]
	private IntPtr gameServerBaseUrl;
    [FieldOffset(32)]
	public int MaxTopPlacers;

    public string? RepoUrl
    {
        get => Marshal.PtrToStringUTF8(repoUrl) ?? string.Empty;
        set => repoUrl = Marshal.StringToHGlobalAuto(value);
    }

    public string DownloadBaseUrl
    {
        get => Marshal.PtrToStringUTF8(downloadBaseUrl) ?? string.Empty;
        set => downloadBaseUrl = Marshal.StringToHGlobalAuto(value);
    }

    public string? CommitHashesFileName
    {
        get => Marshal.PtrToStringUTF8(commitHashesFileName) ?? string.Empty;
        set => commitHashesFileName = Marshal.StringToHGlobalAuto(value);
    }

    public string GameServerBaseUrl
    {
        get => Marshal.PtrToStringUTF8(gameServerBaseUrl) ?? string.Empty;
        set => gameServerBaseUrl = Marshal.StringToHGlobalAuto(value);
    }
}
