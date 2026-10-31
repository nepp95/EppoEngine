namespace EppoScriptCore.Core
{
    public enum CursorMode : byte
    {
        Normal = 0,   // Visible and free
        Hidden = 1,   // Invisible over the window, free to leave it
        Disabled = 2, // Hidden and locked; position reports unbounded deltas
    }
}
