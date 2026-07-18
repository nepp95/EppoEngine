namespace EppoScriptCore.Core
{
    public enum ScriptFieldType : byte
    {
        None = 0,
        Float, Double,
        Bool,
        Char, Int16, Int32, Int64,
        Byte, UInt16, UInt32, UInt64,
        Vector2, Vector3, Vector4,
        Entity,
    }
}