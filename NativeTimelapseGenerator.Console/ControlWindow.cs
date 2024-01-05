using System;
using Terminal.Gui;
using System.Runtime.InteropServices;
using NStack;


[StructLayout(LayoutKind.Explicit, Size = 24)] // 64 bit platform
unsafe struct NativeCanvasInfo
{
    [FieldOffset(0)]
    public IntPtr Url;
    [FieldOffset(8)]
    public IntPtr CommitHash;
    [FieldOffset(16)]
    public long Date;
}

record struct WorkerInfo(string Name, int Count);

// dotnet publish -r linux-x64 -c Release
static unsafe class StaticConsole
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Callback();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void WorkersCountCallback(int workerId, int changeBy);
    private static Callback? shutdownCallback = null;
    private static Callback? startCallback = null;
    private static WorkersCountCallback? addWorkerCallback = null;
    private static WorkersCountCallback? removeWorkerCallback = null;
    private static List<string> serverLogs = new List<string>();
    private static ListView? serverLogListView = null;
    private static readonly object serverLogsLock = new object();

    private static Label? downloadWorkerLabel = null;
    private static Label? renderWorkerLabel = null;
    private static Label? saveWorkerLabel = null;

    private static Label? backupsTotalLabel = null;
    private static Label? backupsPsLabel = null;
    private static Label? currentDateLabel = null;

    [UnmanagedCallersOnly(EntryPoint = "start_console")]
    public static void StartConsole(IntPtr startCallbackPtr, IntPtr shutdownCallbackPtr, IntPtr addWorkerPtr, IntPtr removeWorkerPtr)
    {
        // Callbacks
        shutdownCallback = Marshal.GetDelegateForFunctionPointer<Callback>(shutdownCallbackPtr);
        startCallback = Marshal.GetDelegateForFunctionPointer<Callback>(startCallbackPtr);
        addWorkerCallback = Marshal.GetDelegateForFunctionPointer<WorkersCountCallback>(addWorkerPtr);
        removeWorkerCallback = Marshal.GetDelegateForFunctionPointer<WorkersCountCallback>(removeWorkerPtr);

        // GUI App
        Application.Init();
        var window = new Window()
        {
            X = 0,
            Y = 0,
            Width = Dim.Fill(),
            Height = Dim.Fill(),
            Border = new Border()
            {
                BorderBrush = Color.White,
                BorderStyle = BorderStyle.Rounded,
                Title = "NativeTimelapseGenerator Control Panel (Ctrl+Q to quit)"
            }
        };
        Application.Top.Add(window);

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

        var workerPanel = new View()
        {
            Y = 4,
            X = 1,
            Height = 4,
            Width = Dim.Percent(50) - 2,
            Border = new Border()
            {
                BorderBrush = Color.White,
                BorderStyle = BorderStyle.Rounded,
                Title = "Workers"
            }
        }; 
        var downloadLabel = new Label("Download workers: ")
        {
            Y = 0,
            X = 1,
        };
        downloadWorkerLabel = new Label("0")
        {
            Y = 0,
            X = Pos.Right(downloadLabel),
        };
        var renderLabel = new Label("Render workers: ")
        {
            Y = 1,
            X = 1,
        };
        renderWorkerLabel = new Label("0")
        {
            Y = 1,
            X = Pos.Right(renderLabel),
        };
        var saveLabel = new Label("Save workers: ")
        {
            Y = 2,
            X = 1,
        };
        saveWorkerLabel = new Label("0")
        {
            Y = 2,
            X = Pos.Right(saveLabel),
        };
        var addButton = new Button("Add worker")
        {
            Y = 3,
            X = 0,
        };
        addButton.Clicked += () =>
        {
            var selected = MessageBox.Query("Add worker", "Select step to add single worker to", "Download", "Render", "Save", "Cancel");
            if (selected != 3)
            {
                addWorkerCallback(selected, 1);
            }
        };
        var removeButton = new Button("Rem worker")
        {
            Y = 3,
            X = Pos.Percent(50)
        };
        removeButton.Clicked += () =>
        {
            var selected = MessageBox.Query("Remove worker", "Select step to remove single worker from", "Download", "Render", "Save ", "Cancel");
            if (selected != 3)
            {
                removeWorkerCallback(selected, 1);
            }
        };
        workerPanel.Add(downloadLabel, downloadWorkerLabel, renderLabel, renderWorkerLabel, saveLabel, saveWorkerLabel, addButton, removeButton);

        var backupPanel = new View()
        {
            Y = 4,
            X = Pos.Percent(50) + 2,
            Height = 4,
            Width = Dim.Percent(50) - 2,
            Border = new Border()
            {
                BorderBrush = Color.White,
                BorderStyle = BorderStyle.Rounded,
                Title = "Backups"
            }
        }; 
        var backupsTotal = new Label("Total frames: ")
        {
            Y = 0,
            X = 1,
        };
        backupsTotalLabel = new Label("0")
        {
            Y = 0,
            X = Pos.Right(backupsTotal),
        };
        var backupsPs = new Label("Frames per second: ")
        {
            Y = 1,
            X = 1,
        };
        backupsPsLabel = new Label("0")
        {
            Y = 1,
            X = Pos.Right(backupsPs),
        };
        var currentDate = new Label("Current canvas date: ")
        {
            Y = 2,
            X = 1,
        };
        currentDateLabel = new Label("null")
        {
            Y = 2,
            X = Pos.Right(currentDate),
        };
        backupPanel.Add(backupsTotal, backupsTotalLabel, backupsPs, backupsPsLabel, currentDate, currentDateLabel);

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

        var clearBtn = new Button()
        {
            Text = "Clear",
            Y = Pos.AnchorEnd() - 12,
            X = Pos.AnchorEnd() - 12,
        };
        clearBtn.Clicked += () =>
        {
            lock (serverLogsLock)
            {
                serverLogs.Clear();
            }
        };
        window.Add(shutdownBtn, startBtn, workerPanel, backupPanel, serverLogPanel, clearBtn);

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
            var chunks = SplitChunks(message, 128);
            try
            {
                lock (serverLogsLock)
                {
                    for (var i = 0; i < chunks.Length; i++)
                    {
                        var chunk = chunks[i];
                        if (i != 0)
                        {
                            chunk = "| " + chunk;
                        }
                        serverLogs.Add(chunk);
                    }

                    serverLogListView?.SetNeedsDisplay();
                }
            }
            catch (Exception error)
            {
                File.AppendAllText("logs.txt", $"\n{DateTime.Now} Error while adding UI server log, {error}");
            }
        }
    }

    static string[] SplitChunks(string input, int chunkSize)
    {
        int divResult = Math.DivRem(input.Length, chunkSize, out var remainder);
    
        int chunkCount = remainder > 0 ? divResult + 1 : divResult;
        var result = new string[chunkCount];
    
        int i = 0;
        while (i < chunkCount - 1)
        {
            result[i] = input.Substring(i * chunkSize, chunkSize);
            i++;
        }
    
        int lastChunkSize = remainder > 0 ? remainder : chunkSize;
        result[i] = input.Substring(i * chunkSize, lastChunkSize);
        return result;
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

    [UnmanagedCallersOnly(EntryPoint = "update_worker_stats")]
    public static void UpdateWorkerStats(int workerStep, int count)
    {
        switch (workerStep)
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
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "update_backups_stats")]
    public static void UpdateBackupStats(int backupsTotal, float backupsPerSecond, NativeCanvasInfo currentInfo)
    {
        var date = DateTimeOffset.FromUnixTimeSeconds(currentInfo.Date);
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
        }
    }
}
