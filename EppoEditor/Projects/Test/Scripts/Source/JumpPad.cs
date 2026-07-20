using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace Test
{
    public class JumpPad : Entity
    {
        public float Impulse = 12.0f;
        public float Radius = 1.0f;
        public float Cooldown = 0.4f;

        private Entity? m_Player;
        private float m_CooldownTimer;

        public override void OnCreate()
        {
            Log.Info("JumpPad::OnCreate");
            m_Player = Scene.FindEntityByName("Player");
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Player == null)
                return;

            if (m_CooldownTimer > 0.0f)
            {
                m_CooldownTimer -= deltaTime;
                return;
            }

            Vector3 padPos = Translation;

            if (Physics.OverlapsSphere(m_Player, padPos, Radius))
            {
                Physics.ApplyLinearImpulse(m_Player, new Vector3(0.0f, Impulse, 0.0f));
                m_CooldownTimer = Cooldown;
                Log.Info("JumpPad: boost!");
            }
        }
    }
}
