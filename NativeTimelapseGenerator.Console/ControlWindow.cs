using System;
using Terminal.Gui;
using System.Runtime.InteropServices;

// dotnet publish -r linux-x64 -c Release
static unsafe class StaticConsole
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Callback();
    private static Callback? shutdownCallback = null;
    private static Callback? startCallback = null;
    private static List<string> serverLogs = new List<string>();
    private static ListView? serverLogListView = null;
    private static readonly object serverLogsLock = new object();


    [UnmanagedCallersOnly(EntryPoint = "start_console")]
    public static void StartConsole(IntPtr startCallbackPtr, IntPtr shutdownCallbackPtr)
    {
        // Callbacks
        shutdownCallback = Marshal.GetDelegateForFunctionPointer<Callback>(shutdownCallbackPtr);
        startCallback = Marshal.GetDelegateForFunctionPointer<Callback>(startCallbackPtr);

        // GUI App
        Application.Init();
        var window = new Window("NativeTimelapseGenerator Control Panel (Ctrl+Q to quit)")
        {
            X = 0,
            Y = 0,
            Width = Dim.Fill(),
            Height = Dim.Fill()
        };
        Application.Top.Add(window);

        serverLogListView = new ListView(new Rect(0, 0, 128, 10), serverLogs)
        {
            Width = Dim.Fill()
        };
        var serverLogPanel = new PanelView
        {
            Y = Pos.AnchorEnd() - 12,
            Height = 10,
            Border = new Border
            {
                BorderBrush = Color.White,
                BorderStyle = BorderStyle.Rounded,
                Title = "Server logs"
            },
            ColorScheme = Colors.Base,
            Child = serverLogListView
        };

        var shutdownBtn = new Button()
        {
            Text = "Shutdown",
            Y = 1,
            X = Pos.Center() + 6,
        };
        shutdownBtn.Clicked += () =>
        {
            MessageBox.Query("Shutdown", "Halting backups generation", "Okay");
            Application.Shutdown();
            shutdownCallback?.Invoke();
        };

        var startBtn = new Button()
        {
            Text = "Start",
            Y = 1,
            X = Pos.Center() - 12,
        };
        startBtn.Clicked += () =>
        {
            MessageBox.Query("Start", "Starting backups generation", "Okay");
            startCallback?.Invoke();
        };
        window.Add(shutdownBtn, startBtn, serverLogPanel);

        AppDomain.CurrentDomain.UnhandledException += (sender, args) =>
        {
            lock (serverLogsLock)
            {
                serverLogs.Add("[ui] Error - " + args.ToString());
                serverLogListView?.SetNeedsDisplay();
            }
        };

        try
        {
            Application.Run();
        }
        catch (Exception error)
        {
            File.AppendAllText("logs.txt", $"\n{DateTime.Now} Error while running UI application, {error}");
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "log_message")]
    public static void LogMessage(IntPtr messageChars)
    {
        var message = Marshal.PtrToStringUTF8(messageChars);
        if (message != null)
        {
            try
            {
                lock (serverLogsLock)
                {
                    serverLogs.Add(message);
                    serverLogListView?.SetNeedsDisplay();
                }
            }
            catch (Exception error)
            {
                File.AppendAllText("logs.txt", $"\n{DateTime.Now} Error while adding UI server log, {error}");
            }
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "stop_console")]
    public static void StopConsole()
    {
        try
        {
            Application.Shutdown();
        }
        catch (Exception error)
        {
            File.AppendAllText("logs.txt", $"\n{DateTime.Now} Error while shutting down UI application, {error}");
        }
        shutdownCallback?.Invoke();
    }
}
