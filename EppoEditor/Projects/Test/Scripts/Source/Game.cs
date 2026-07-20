using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace Test
{
    public class Player : Entity
    {
        public float MoveSpeed = 6.0f;
        public float JumpForce = 7.0f;

        private const float JumpBufferTime = 0.10f;
        private const float GroundCheckDistance = 1.1f;
        private const float GroundMinNormalY = 0.7f;

        private Vector3 m_SpawnPoint;
        private Entity? m_Camera;
        private bool m_WasSpacePressed;
        private float m_JumpBufferTimer;

        private RigidBodyComponent? m_RigidBody;

        public override void OnCreate()
        {
            Log.Info("Player::OnCreate");
            m_SpawnPoint = Translation;
            m_Camera = Scene.FindEntityByName("FPS Camera");
            m_RigidBody = GetComponent<RigidBodyComponent>();
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Camera == null || m_RigidBody == null)
                return;

            // Build movement basis from the camera's yaw. The camera looks down -Z
            // at yaw=0; rotating that basis vector by yaw gives forward, +X gives right.
            Vector3 camRot = m_Camera.GetComponent<TransformComponent>().Rotation;
            float yaw = camRot.Y;
            Vector3 forward = new(-MathF.Sin(yaw), 0.0f, -MathF.Cos(yaw));
            Vector3 right = new(MathF.Cos(yaw), 0.0f, -MathF.Sin(yaw));

            Vector3 direction = new(0.0f, 0.0f, 0.0f);
            if (Input.IsKeyPressed(KeyCode.W))
                direction += forward;
            if (Input.IsKeyPressed(KeyCode.S))
                direction -= forward;
            if (Input.IsKeyPressed(KeyCode.A))
                direction -= right;
            if (Input.IsKeyPressed(KeyCode.D))
                direction += right;

            if (direction.X != 0.0f || direction.Z != 0.0f)
            {
                float len = MathF.Sqrt(direction.X * direction.X + direction.Z * direction.Z);
                direction.X /= len;
                direction.Z /= len;
            }

            Vector3 velocity = m_RigidBody.LinearVelocity;
            m_RigidBody.LinearVelocity = new Vector3(direction.X * MoveSpeed, velocity.Y, direction.Z * MoveSpeed);

            // Grounded via a ray cast down from the capsule center. The ray starts
            // inside the player's own convex capsule, which box3d ignores, so it
            // only reports the ground below. A mostly-upward normal means a
            // walkable surface, not a wall.
            bool grounded = Physics.Raycast(Translation, new Vector3(0.0f, -1.0f, 0.0f), GroundCheckDistance, out RaycastHit groundHit)
                && groundHit.Normal.Y > GroundMinNormalY;

            // Edge-detect Space and buffer the press so a tap just before landing
            // still triggers once grounded is recognized a frame or two later.
            bool spaceHeld = Input.IsKeyPressed(KeyCode.Space);

            if (spaceHeld && !m_WasSpacePressed)
                m_JumpBufferTimer = JumpBufferTime;
            m_WasSpacePressed = spaceHeld;

            if (m_JumpBufferTimer > 0.0f)
                m_JumpBufferTimer -= deltaTime;

            if (m_JumpBufferTimer > 0.0f && grounded)
            {
                m_RigidBody.LinearVelocity = new Vector3(direction.X * MoveSpeed, JumpForce, direction.Z * MoveSpeed);
                m_JumpBufferTimer = 0.0f;
            }

            // Safety net: if the player slips off the parcours edge, relaunch them
            // up and toward spawn so they can walk back onto the course.
            if (Translation.Y < -8.0f)
            {
                Vector3 toSpawn = new(m_SpawnPoint.X - Translation.X, 0.0f, m_SpawnPoint.Z - Translation.Z);
                float d = MathF.Sqrt(toSpawn.X * toSpawn.X + toSpawn.Z * toSpawn.Z);
                
                if (d > 0.001f)
                {
                    toSpawn.X /= d;
                    toSpawn.Z /= d;
                }
                
                m_RigidBody.LinearVelocity = new Vector3(toSpawn.X * MoveSpeed, 8.0f, toSpawn.Z * MoveSpeed);
            }
        }

        public override void OnDestroy()
        {
            Log.Info("Player::OnDestroy");
        }
    }
}
