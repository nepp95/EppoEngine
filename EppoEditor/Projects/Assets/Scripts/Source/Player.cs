using Eppo;

namespace Sandbox
{
	public class Player : Entity
	{
		private RigidBodyComponent RigidBody;
		
		void OnCreate()
		{
			RigidBody = GetComponent<RigidBodyComponent>();
		}

		void OnUpdate(float timestep)
		{
			
		}
	}
}
