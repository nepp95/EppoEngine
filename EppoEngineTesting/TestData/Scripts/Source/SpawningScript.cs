using EppoScriptCore.Core;
using EppoScriptCore.Scene;

namespace EppoTesting
{
    // Spawned components are left without a class name on purpose: it keeps the
    // spawns from recursing.
    public class SpawningScript : Entity
    {
        public const int SpawnCount = 8;

        private static void SpawnScripted(string name)
        {
            Entity spawned = Scene.CreateEntity(name);
            InternalCalls.Entity_AddComponent(spawned.ID, "ScriptComponent");
        }

        public override void OnCreate()
        {
            for (int i = 0; i < SpawnCount; i++)
                SpawnScripted("SpawnedFromCreate" + i);
        }

        public override void OnDestroy()
        {
            for (int i = 0; i < SpawnCount; i++)
                SpawnScripted("SpawnedFromDestroy" + i);
        }
    }
}
