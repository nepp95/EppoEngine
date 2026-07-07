using EppoScriptCore;
using EppoScriptCore.Core;
using EppoScriptCore.ECS;

namespace Test
{
    public class Player : ScriptBehaviour
    {
        public float Speed = 5.0f;
        public Entity Target;

        public override void OnCreate()
        {
            Log.Info("Player::OnCreate");
        }

        public override void OnUpdate(float deltaTime)
        {
            // Basic movement stub for testing
        }

        public override void OnDestroy()
        {
            Log.Info("Player::OnDestroy");
        }

        public void Respawn()
        {
            Log.Info("Player::Respawn");
        }
    }
}
