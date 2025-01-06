using System.Runtime.InteropServices;

namespace NativeTimelapseGenerator.Console.Delegates;

public class CallbackDelegate
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Callback();

    private readonly Callback? callback;

    public CallbackDelegate(IntPtr callbackPtr)
    {
        callback = Marshal.GetDelegateForFunctionPointer<Callback>(callbackPtr);
    }

    public void Invoke()
    {
        callback?.Invoke();
    }
}