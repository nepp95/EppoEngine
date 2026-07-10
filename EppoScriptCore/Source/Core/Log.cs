using System.Runtime.InteropServices;

namespace EppoScriptCore.Core
{
    public enum LogLevel : byte
    {
        Trace = 0,
        Info,
        Warn,
        Error
    }

    public static class Log
    {
        public static void Trace(string message) => Invoke(LogLevel.Trace, message);
        public static void Info(string message) => Invoke(LogLevel.Info, message);
        public static void Warn(string message) => Invoke(LogLevel.Warn, message);
        public static void Error(string message) => Invoke(LogLevel.Error, message);

        private static void Invoke(LogLevel level, string message)
        {
            if (NativeCallbacks.LogCallback is null)
                return;

            var ptr = Marshal.StringToCoTaskMemUTF8(message);
            NativeCallbacks.LogCallback((byte)level, ptr);
            Marshal.FreeCoTaskMem(ptr);
        }
    }
}