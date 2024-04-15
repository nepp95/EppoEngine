namespace Eppo
{
	public class Input
	{
		public static bool IsKeyPressed(KeyCode keyCode)
		{
			return InternalCalls.Input_IsKeyPressed(keyCode);
		}
	}
}
