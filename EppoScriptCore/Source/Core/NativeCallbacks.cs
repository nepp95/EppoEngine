using System.Runtime.InteropServices;

namespace EppoScriptCore.Core
{
    public static class NativeCallbacks
    {
        internal delegate void LogCallbackDelegate(byte level, IntPtr message);
        internal static LogCallbackDelegate? LogCallback;

        [StructLayout(LayoutKind.Sequential)]
        private struct Table
        {
            public IntPtr Log;
        }

        internal static void Register(IntPtr tablePtr)
        {
            if (tablePtr == IntPtr.Zero)
                return;

            Table table = Marshal.PtrToStructure<Table>(tablePtr);

            LogCallback = Bind<LogCallbackDelegate>(table.Log);
        }

        private static T? Bind<T>(IntPtr ptr) where T : Delegate => ptr == IntPtr.Zero ? null : Marshal.GetDelegateForFunctionPointer<T>(ptr);
    }
}