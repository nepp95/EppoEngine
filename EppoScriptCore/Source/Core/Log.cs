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
        public static void Trace(string message) => InternalCalls.LogMessage((byte)LogLevel.Trace, message);
        public static void Info(string message) => InternalCalls.LogMessage((byte)LogLevel.Info, message);
        public static void Warn(string message) => InternalCalls.LogMessage((byte)LogLevel.Warn, message);
        public static void Error(string message) => InternalCalls.LogMessage((byte)LogLevel.Error, message);
    }
}
