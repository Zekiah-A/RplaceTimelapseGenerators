using System.Runtime.InteropServices;
using NativeTimelapseGenerator.Console.Structs;

namespace NativeTimelapseGenerator.Console.Delegates;

public class StartCallbackDelegate
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void StartCallback(Config config);

    private readonly StartCallback? callback;
    
    public StartCallbackDelegate(IntPtr callbackPtr)
    {
        callback = Marshal.GetDelegateForFunctionPointer<StartCallback>(callbackPtr);
    }
    
    public void Invoke(Config config)
    {
        callback?.Invoke(config);
    }
}