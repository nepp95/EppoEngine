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

        // Mouse position relative to the top-left of the viewport the scene renders
        // into (the editor's viewport panel, or the whole window in a packaged game).
        public static Vector2 GetViewportMousePosition()
            => InternalCalls.Input_GetViewportMousePosition();

        public static Vector2 GetViewportSize()
            => InternalCalls.Input_GetViewportSize();

        public static CursorMode CursorMode
        {
            get => (CursorMode)InternalCalls.Input_GetCursorMode();
            set => InternalCalls.Input_SetCursorMode((byte)value);
        }
    }
}
