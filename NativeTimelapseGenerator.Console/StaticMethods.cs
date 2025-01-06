using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using NativeTimelapseGenerator.Console.Delegates;
using NativeTimelapseGenerator.Console.Structs;
using Terminal.Gui;

namespace NativeTimelapseGenerator.Console;

public static class StaticMethods
{
    private static ControlWindow? instance;

    [UnmanagedCallersOnly(EntryPoint = "start_console")]
    [RequiresUnreferencedCode ("Calls Terminal.Gui.Application.Init(IConsoleDriver, String)")]
    [RequiresDynamicCode ("Calls Terminal.Gui.Application.Init(IConsoleDriver, String)")]
    public static void StartConsole(IntPtr startCallbackPtr, IntPtr shutdownCallbackPtr, IntPtr addWorkerPtr, IntPtr removeWorkerPtr)
    {
        try
        {
            // Callbacks
            var startDelegate = new StartCallbackDelegate(startCallbackPtr);
            var shutdownDelegate = new CallbackDelegate(shutdownCallbackPtr);
            var addWorkerDelegate = new WorkersCountCallbackDelegate(addWorkerPtr);
            var removeWorkerDelegate = new WorkersCountCallbackDelegate(removeWorkerPtr);
            
            // Start GUI application
            Application.Init();
            instance = new ControlWindow(startDelegate, shutdownDelegate, addWorkerDelegate, removeWorkerDelegate);
            Application.Run(instance);
            
            instance.Dispose();
            Application.Shutdown();
        }
        catch (Exception error)
        {
            var errorString = $"{DateTime.Now} Error while running UI application, {error}";
            System.Console.Error.WriteLine(errorString);
            File.AppendAllText("logs.txt", errorString + "\n");
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "log_message")]
    public static void LogMessage(IntPtr messagePtr)
    {
        var message = Marshal.PtrToStringUTF8(messagePtr);
        if (message == null)
        {
            return;
        }
        
        var chunks = message.SplitChunks(128);
        try
        {
            for (var i = 0; i < chunks.Length; i++)
            {
                var chunk = chunks[i];
                if (i != 0)
                {
                    chunk = "| " + chunk;
                }
                instance?.ServerLogs.Add(chunk);
            }
        }
        catch (Exception error)
        {
            File.AppendAllText("logs.txt", $"\n{DateTime.Now} Error while adding UI server log, {error}");
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "stop_console")]
    public static void StopConsole()
    {
        try
        {
            instance?.Dispose();
            Application.Shutdown();
        }
        catch (Exception error)
        {
            File.AppendAllText("logs.txt", $"\n{DateTime.Now} Error while shutting down UI application, {error}");
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "update_worker_stats")]
    public static void UpdateWorkerStats(int workerStep, int count)
    {
        /*switch (workerStep)
        {
            case 0:
                if (downloadWorkerLabel != null)
                {
                    downloadWorkerLabel.Text = count.ToString();
                }
                break;
            case 1:
                if (renderWorkerLabel != null)
                {
                    renderWorkerLabel.Text = count.ToString();
                }
                break;
            case 2:
                if (saveWorkerLabel != null)
                {
                    saveWorkerLabel.Text = count.ToString();
                }
                break;
        }*/
    }

    [UnmanagedCallersOnly(EntryPoint = "update_backups_stats")]
    public static void UpdateBackupStats(int backupsTotal, float backupsPerSecond, CanvasInfo currentInfo)
    {
        /*var date = DateTimeOffset.FromUnixTimeSeconds(currentInfo.Date);
        if (backupsTotalLabel != null)
        {
            backupsTotalLabel.Text = backupsTotal.ToString();
        }
        if (backupsPsLabel != null)
        {
            backupsPsLabel.Text = backupsPerSecond.ToString("0.000");
        }
        if (currentDateLabel != null)
        {
            currentDateLabel.Text = date.ToLocalTime().ToString();
        }*/
    }
}