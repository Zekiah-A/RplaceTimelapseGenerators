using NativeTimelapseGenerator.Console;
using NativeTimelapseGenerator.Console.Delegates;
using NativeTimelapseGenerator.Console.Structs;
using Terminal.Gui;

namespace NativeTimelapseGenerator.ConsoleTest;

internal static class Program
{
    private static ControlWindow? instance;
    
    private static void Start(Config config)
    {
        instance?.ServerLogs.Add($"Native start callback invoked with {ObjectDumper.Dump(config)}");
    }

    private static void Shutdown()
    {
        instance?.ServerLogs.Add("Native shutdown callback invoked");
    }

    private static void AddWorker(int workerId, int changeBy)
    {
        instance?.ServerLogs.Add($"Native add worker callback invoked with {workerId} {changeBy}");
    }

    private static void RemoveWorker(int workerId, int changeBy)
    {
        instance?.ServerLogs.Add($"Native remove callback invoked with {workerId} {changeBy}");
    }

    private static unsafe void Main(string[] args)
    {
        Application.Init();

        delegate*<Config, void> p1 = &Start;
        var d1 = new StartCallbackDelegate((IntPtr)p1);
        
        delegate*<void> p2 = &Shutdown;
        var d2 = new CallbackDelegate((IntPtr)p2);
        
        delegate*<int, int, void> p3 = &AddWorker;
        var d3 = new WorkersCountCallbackDelegate((IntPtr)p3);
        
        delegate*<int, int, void> p4 = &RemoveWorker;
        var d4 = new WorkersCountCallbackDelegate((IntPtr)p4);

        instance = new ControlWindow(d1, d2, d3, d4);
        Application.Run(instance);
    }
}