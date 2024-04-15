namespace Eppo
{
	public enum LogLevel
	{
		Trace = 0,
		Info,
		Warn,
		Error
	}

	public class Log
	{
		public static void Trace(string message)
		{
			InternalCalls.Log(LogLevel.Trace, message);
		}

		public static void Info(string message)
		{
			InternalCalls.Log(LogLevel.Info, message);
		}

		public static void Warn(string message)
		{
			InternalCalls.Log(LogLevel.Warn, message);
		}

		public static void Error(string message)
		{
			InternalCalls.Log(LogLevel.Error, message);
		}
	}
}
