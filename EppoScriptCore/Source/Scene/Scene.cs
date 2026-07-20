using EppoScriptCore.Core;

namespace EppoScriptCore.Scene
{
    // Entry point for spawning and despawning entities from script code against the
    // active runtime scene. CreateEntity yields a bare entity (no ScriptComponent);
    // build it up with entity.AddComponent<T>().
    public static class Scene
    {
        public static Entity CreateEntity(string name)
            => new Entity(InternalCalls.Scene_CreateEntity(name));

        public static void DestroyEntity(Entity entity)
            => InternalCalls.Scene_DestroyEntity(entity.ID);

        public static Entity? FindEntityByName(string name)
        {
            var id = InternalCalls.Scene_FindEntityByName(name);
            return id != 0 ? new Entity(id) : null;
        }
    }
}
