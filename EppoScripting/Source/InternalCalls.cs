using System.Runtime.CompilerServices;

namespace Eppo
{
	public static class InternalCalls
	{
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Log(LogLevel logLevel, string message);
	}
}
