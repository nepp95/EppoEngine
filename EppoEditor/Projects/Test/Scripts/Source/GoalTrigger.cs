using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace Test
{
    public class GoalTrigger : Entity
    {
        public float Radius = 2.0f;

        private const float TargetScale = 1.5f;
        private const float TargetLightIntensity = 25.0f;
        private const float AnimRate = 4.0f;
        private const float PulseAmplitude = 0.3f;
        private const float BobAmplitude = 0.5f;
        private const float OscFreq = 1.5f;
        private const float OscRampTime = 1.0f;

        private Entity? m_Player;
        private Entity? m_GoalLight;
        private bool m_Reached;
        private float m_VictoryTime;
        private float m_BaseY;

        public override void OnCreate()
        {
            Log.Info("GoalTrigger::OnCreate");
            m_Player = Scene.FindEntityByName("Player");
            m_GoalLight = Scene.FindEntityByName("Goal Light");
            m_BaseY = Translation.Y;
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Player != null && !m_Reached)
            {
                Vector3 goalPos = Translation;

                // Real physics overlap: the player's capsule against a sphere at the goal.
                if (Physics.OverlapsSphere(m_Player, goalPos, Radius))
                {
                    m_Reached = true;
                    Log.Info("Parcours complete!");
                }
            }

            if (!m_Reached)
                return;

            m_VictoryTime += deltaTime;

            // After the initial ease-in, layer in a sinusoidal pulse and bob.
            // Ramps in over OscRampTime so the transition from ease to oscillation
            // is seamless rather than a snap.
            float ramp = MathF.Min(m_VictoryTime / OscRampTime, 1.0f);
            float phase = m_VictoryTime * OscFreq * MathF.PI * 2.0f;
            float pulse = MathF.Sin(phase) * PulseAmplitude * ramp;
            float bob = MathF.Sin(phase) * BobAmplitude * ramp;

            float t = 1.0f - MathF.Exp(-AnimRate * deltaTime);

            var transform = GetComponent<TransformComponent>();
            float targetScale = TargetScale + pulse;
            Vector3 scale = transform.Scale;
            scale.X += (targetScale - scale.X) * t;
            scale.Y += (targetScale - scale.Y) * t;
            scale.Z += (targetScale - scale.Z) * t;
            transform.Scale = scale;

            Vector3 pos = transform.Translation;
            pos.Y += ((m_BaseY + bob) - pos.Y) * t;
            transform.Translation = pos;

            if (m_GoalLight != null)
            {
                var light = m_GoalLight.GetComponent<PointLightComponent>();
                if (light != null)
                    light.Intensity += (TargetLightIntensity - light.Intensity) * t;
            }
        }
    }
}
