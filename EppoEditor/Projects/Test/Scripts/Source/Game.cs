using EppoScriptCore;
using EppoScriptCore.Core;
using EppoScriptCore.ECS;
using EppoScriptCore.Math;

namespace Test
{
    public class Player : ScriptBehaviour
    {
        public float Speed = 5.0f;
        public Entity Target;

        // GLFW keycode for Space.
        private const uint KeySpace = 32;

        public override void OnCreate()
        {
            Log.Info("Player::OnCreate");

            // Kick the body upward at start to show physics driven from a script.
            GetComponent<RigidBody>().ApplyLinearImpulse(new Vector3(0.0f, 5.0f, 0.0f));
        }

        public override void OnUpdate(float deltaTime)
        {
            // Press Space to jump: overwrite vertical velocity, keep horizontal.
            if (Input.IsKeyDown(KeySpace))
            {
                RigidBody body = GetComponent<RigidBody>();
                Vector3 velocity = body.LinearVelocity;
                body.LinearVelocity = new Vector3(velocity.X, 8.0f, velocity.Z);
            }
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
