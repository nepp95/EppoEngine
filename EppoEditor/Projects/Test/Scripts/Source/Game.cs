using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace Test
{
    public class Player : Entity
    {
        public float Speed = 5.0f;
        public Entity Target;

        public override void OnCreate()
        {
            Log.Info("Player::OnCreate");
            Physics.ApplyLinearImpulse(this, new Vector3(0.0f, 5.0f, 0.0f));
        }

        public override void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyPressed(KeyCode.Space))
            {
                RigidBodyComponent body = GetComponent<RigidBodyComponent>();
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
