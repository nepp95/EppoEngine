using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace Chess
{
    // Moves an entity to a target over time along a small arc, optionally scaling
    // it and adding a rotation on the way. Returns true from Advance when done.
    public sealed class Tween
    {
        private readonly TransformComponent m_Transform;
        private readonly Vector3 m_FromPosition;
        private readonly Vector3 m_ToPosition;
        private readonly Vector3 m_FromRotation;
        private readonly Vector3 m_ToRotation;
        private readonly Vector3 m_FromScale;
        private readonly Vector3 m_ToScale;
        private readonly float m_Duration;
        private readonly float m_ArcHeight;
        private float m_Elapsed;

        public Tween(Entity entity, Vector3 target, float duration, float arcHeight, float scale = 1.0f, Vector3? addRotation = null)
        {
            m_Transform = entity.GetComponent<TransformComponent>()!;
            m_FromPosition = m_Transform.Translation;
            m_ToPosition = target;
            m_FromRotation = m_Transform.Rotation;
            m_ToRotation = m_FromRotation + (addRotation ?? Vector3.Zero);
            m_FromScale = m_Transform.Scale;
            m_ToScale = new Vector3(scale);
            m_Duration = MathF.Max(duration, 1e-3f);
            m_ArcHeight = arcHeight;
        }

        public bool Advance(float timestep)
        {
            m_Elapsed = MathF.Min(m_Elapsed + timestep, m_Duration);
            float t = m_Elapsed / m_Duration;
            float eased = t * t * (3.0f - 2.0f * t);

            Vector3 position = Lerp(m_FromPosition, m_ToPosition, eased);
            position.Y += m_ArcHeight * 4.0f * t * (1.0f - t);

            m_Transform.Translation = position;
            m_Transform.Rotation = Lerp(m_FromRotation, m_ToRotation, eased);
            m_Transform.Scale = Lerp(m_FromScale, m_ToScale, eased);

            return m_Elapsed >= m_Duration;
        }

        private static Vector3 Lerp(Vector3 a, Vector3 b, float t) => a + (b - a) * t;
    }
}
