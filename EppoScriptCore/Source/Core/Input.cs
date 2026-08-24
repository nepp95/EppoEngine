using EppoScriptCore.Math;

namespace EppoScriptCore.Core
{
    public static class Input
    {
        public static bool IsKeyPressed(KeyCode key)
            => InternalCalls.Input_IsKeyPressed((ushort)key);

        public static bool IsMouseButtonPressed(MouseCode button)
            => InternalCalls.Input_IsMouseButtonPressed((ushort)button);

        public static Vector2 GetMousePosition()
            => InternalCalls.Input_GetMousePosition();

        public static float GetMouseX()
            => GetMousePosition().X;

        public static float GetMouseY()
            => GetMousePosition().Y;
    }
}
