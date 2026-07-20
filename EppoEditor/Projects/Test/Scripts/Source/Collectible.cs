using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace Test
{
    public class Collectible : Entity
    {
        public float Radius = 1.2f;
        public float SpinSpeed = 2.0f;

        private Entity? m_Player;
        private bool m_Collected;
        private float m_Angle;

        public override void OnCreate()
        {
            Log.Info("Collectible::OnCreate");
            m_Player = Scene.FindEntityByName("Player");
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Collected || m_Player == null)
                return;

            m_Angle += deltaTime * SpinSpeed;
            var transform = GetComponent<TransformComponent>();
            transform.Rotation = new Vector3(0.0f, m_Angle, 0.0f);

            Vector3 orbPos = Translation;

            if (Physics.OverlapsSphere(m_Player, orbPos, Radius))
            {
                m_Collected = true;
                transform.Scale = new Vector3(0.0f, 0.0f, 0.0f);
                Log.Info("Collectible grabbed!");
            }
        }
    }
}
