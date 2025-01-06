using System.Runtime.InteropServices;

namespace NativeTimelapseGenerator.Console.Delegates;

public class WorkersCountCallbackDelegate
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void WorkersCountCallback(int workerId, int changeBy);

    private readonly WorkersCountCallback? callback;

    public WorkersCountCallbackDelegate(IntPtr callbackPtr)
    {
        callback = Marshal.GetDelegateForFunctionPointer<WorkersCountCallback>(callbackPtr);
    }

    public void Invoke(int workerId, int changeBy)
    {
        callback?.Invoke(workerId, changeBy);
    }
}