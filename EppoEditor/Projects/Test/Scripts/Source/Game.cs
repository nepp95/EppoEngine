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

        public override void OnCreate()
        {
            Log.Info("Player::OnCreate");
            GetComponent<RigidBody>().ApplyLinearImpulse(new Vector3(0.0f, 5.0f, 0.0f));
        }

        public override void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyDown(KeyCode.Space))
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
