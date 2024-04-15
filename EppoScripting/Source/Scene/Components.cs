namespace Eppo
{
	public abstract class Component
	{
		public Entity Entity { get; internal set; }
	}

	public class TransformComponent : Component
	{
		public Vector3 Translation
		{
			get
			{
				InternalCalls.TransformComponent_GetTranslation(Entity.ID, out Vector3 translation);
				return translation;
			}
			set => InternalCalls.TransformComponent_SetTranslation(Entity.ID, ref value);
		}
	}

	public class RigidBodyComponent : Component
	{
		public void ApplyLinearImpulse(Vector3 impulse, Vector3 worldPosition)
		{
			InternalCalls.RigidBodyComponent_ApplyLinearImpulse(Entity.ID, ref impulse, ref worldPosition);
		}

		public void ApplyLinearImpulse(Vector3 impulse)
		{
			InternalCalls.RigidBodyComponent_ApplyLinearImpulseToCenter(Entity.ID, ref impulse);
		}
	}
}
