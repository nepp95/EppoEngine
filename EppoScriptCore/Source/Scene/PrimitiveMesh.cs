namespace EppoScriptCore.Scene
{
    // Reserved asset handles for the built-in meshes, generated on demand rather
    // than registered. Values must stay in sync with the native MeshPrimitiveType.
    public enum PrimitiveMesh : ulong
    {
        Cone = 1,
        Cube = 2,
        Cylinder = 3,
        Sphere = 4,
        Capsule = 5,
    }
}
