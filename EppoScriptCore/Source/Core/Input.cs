namespace EppoScriptCore.Core
{
    public static class Input
    {
        public static bool IsKeyPressed(KeyCode key)
            => InternalCalls.Input_IsKeyPressed((ushort)key);
    }
}
