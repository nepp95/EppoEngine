using Eppo;

namespace Sandbox
{
	public class Player : Entity
	{
		public float Timestep = 0.0f;
		public int Count = 0;

		void OnCreate()
		{
			Log.Info($"Created player entity from C# with UUID: {ID}");
		}

		void OnUpdate(float timestep)
		{
			Timestep += timestep;

			if (Timestep - Count > 1.0f)
			{
				Log.Info($"Tickie tick! {Count} ticks.");
				Count++;
			}
		}
	}
}
