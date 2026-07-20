using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace Test
{
    public class FirstPersonCamera : Entity
    {
        public float MouseSensitivity = 0.0025f;
        public float EyeHeight = 0.7f;

        private const float OrbitRadius = 7.0f;
        private const float OrbitHeight = 5.0f;
        private const float OrbitSpeed = 0.4f;
        private const float CompletedThreshold = 1.0f;

        private Entity? m_Target;
        private Entity? m_Goal;
        private float m_Yaw;
        private float m_Pitch;
        private Vector2 m_LastMouse;
        private bool m_MouseInitialized;
        private bool m_VictoryMode;
        private float m_OrbitAngle;

        public override void OnCreate()
        {
            Log.Info("FirstPersonCamera::OnCreate");
            m_Target = Scene.FindEntityByName("Player");
            m_Goal = Scene.FindEntityByName("Goal Trigger");
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Target == null)
                return;

            // Detect completion by observing the goal orb's scale, which grows
            // past its initial 0.5 only after GoalTrigger fires.
            if (!m_VictoryMode && m_Goal != null)
            {
                Vector3 goalScale = m_Goal.GetComponent<TransformComponent>().Scale;
                if (goalScale.X > CompletedThreshold)
                {
                    m_VictoryMode = true;
                    Log.Info("Camera: switching to victory orbit");
                }
            }

            if (m_VictoryMode)
            {
                UpdateVictoryOrbit(deltaTime);
                return;
            }

            UpdateFirstPerson(deltaTime);
        }

        private void UpdateFirstPerson(float deltaTime)
        {
            Vector2 mouse = Input.GetMousePosition();
            if (!m_MouseInitialized)
            {
                m_LastMouse = mouse;
                m_MouseInitialized = true;
                return;
            }

            float dx = mouse.X - m_LastMouse.X;
            float dy = mouse.Y - m_LastMouse.Y;
            m_LastMouse = mouse;

            m_Yaw -= dx * MouseSensitivity;
            m_Pitch -= dy * MouseSensitivity;
            const float pitchLimit = 1.5f;
            if (m_Pitch > pitchLimit) m_Pitch = pitchLimit;
            if (m_Pitch < -pitchLimit) m_Pitch = -pitchLimit;

            var transform = GetComponent<TransformComponent>();
            transform.Rotation = new Vector3(m_Pitch, m_Yaw, 0.0f);

            Vector3 targetPos = m_Target.Translation;
            transform.Translation = new Vector3(targetPos.X, targetPos.Y + EyeHeight, targetPos.Z);
        }

        private void UpdateVictoryOrbit(float deltaTime)
        {
            if (m_Goal == null)
                return;

            m_OrbitAngle += OrbitSpeed * deltaTime;

            Vector3 center = m_Goal.Translation;
            float x = center.X + MathF.Sin(m_OrbitAngle) * OrbitRadius;
            float z = center.Z + MathF.Cos(m_OrbitAngle) * OrbitRadius;
            float y = center.Y + OrbitHeight;

            var transform = GetComponent<TransformComponent>();
            transform.Translation = new Vector3(x, y, z);

            // Look at the goal orb.
            Vector3 forward = new Vector3(center.X - x, center.Y - y, center.Z - z);
            float yaw = MathF.Atan2(-forward.X, -forward.Z);
            float horizontalDist = MathF.Sqrt(forward.X * forward.X + forward.Z * forward.Z);
            float pitch = MathF.Atan2(forward.Y, horizontalDist);
            transform.Rotation = new Vector3(pitch, yaw, 0.0f);
        }
    }
}
