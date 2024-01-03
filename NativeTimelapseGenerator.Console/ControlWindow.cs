using System;
using Terminal.Gui;
using System.Runtime.InteropServices;

// dotnet publish -r linux-x64 -c Release
static class StaticConsole
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ShutdownCallback();
    public static ShutdownCallback? shutdownCallback = null;

    [UnmanagedCallersOnly(EntryPoint = "start_console")]
    public static unsafe void StartConsole(IntPtr shutdownCallbackPtr)
    {
        Application.Run<ControlWindow>();
        shutdownCallback = Marshal.GetDelegateForFunctionPointer<ShutdownCallback>(shutdownCallbackPtr);
        Application.Shutdown();
        shutdownCallback();
    }

    [UnmanagedCallersOnly(EntryPoint = "stop_console")]
    public static unsafe void StartConsole(IntPtr shutdownCallbackPtr)
    {
        Application.Shutdown();
        shutdownCallback();
    }

}

public class ControlWindow : Window
{
    public ControlWindow()
    {
        Title = "NativeTimelapseGenerator Control Panel (Ctrl+Q to quit)";

        var usernameLabel = new Label()
        {
            Text = "Username:"
        };

        var usernameText = new TextField("")
        {
            X = Pos.Right(usernameLabel) + 1,
            Width = Dim.Fill(),
        };

        var btnShutdown = new Button()
        {
            Text = "Shutdown",
            Y = Pos.Bottom(passwordLabel) + 1,
            X = Pos.Center(),
            IsDefault = true,
        };

        btnShutdown.Clicked += () =>
        {
            MessageBox.Query("Shutdown", "Halting backups generation", "Okay");
            Application.RequestStop();
        };

        Add(usernameLabel, usernameText, passwordLabel, passwordText, btnShutdown);
    }
}