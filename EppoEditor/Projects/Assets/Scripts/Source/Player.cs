using Eppo;

namespace Sandbox
{
	public class Player : Entity
	{
		private RigidBodyComponent m_RigidBody;

		public float Speed = 0.0f;

		void OnCreate()
		{
			m_RigidBody = GetComponent<RigidBodyComponent>();
		}

		void OnUpdate(float timestep)
		{
			float speed = Speed;
			Vector3 velocity = Vector3.Zero;

			if (Input.IsKeyPressed(KeyCode.W))
				velocity.Z = 1.0f;
			else if (Input.IsKeyPressed(KeyCode.S))
				velocity.Z = -1.0f;
			if (Input.IsKeyPressed(KeyCode.A))
				velocity.X = -1.0f;
			else if (Input.IsKeyPressed(KeyCode.D))
				velocity.X = 1.0f;

			if (Input.IsKeyPressed(KeyCode.Space))
				velocity.Y = 1.0f;
			else if (Input.IsKeyPressed(KeyCode.LeftControl))
				velocity.Y = -1.0f;

			velocity *= speed * timestep;

			m_RigidBody.ApplyLinearImpulse(velocity);
		}
	}
}
