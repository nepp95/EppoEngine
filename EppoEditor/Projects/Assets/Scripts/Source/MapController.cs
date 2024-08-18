using Eppo;
using System;
using System.Collections.Generic;

namespace Sandbox
{
	public class MapController : Entity
	{
		private List<Entity> m_Tiles = new List<Entity>();

		void OnCreate()
		{
			List<int> rowSize = new List<int>(){ 4, 5, 6, 7, 6, 5, 4 };

			float hexSize = 1.05f;
			float width = (float)(Math.Sqrt(3) * hexSize);
			float height = 1.5f * hexSize;

			float totalHeight = (rowSize.Count - 1) * height;

			for (int r = 0; r < rowSize.Count; ++r)
			{
				int numCols = rowSize[r];
				float xOffset = -(numCols - 1) * width / 2.0f;
				float z = r * height - totalHeight / 2.0f;

				for (int q = 0; q < numCols; ++q)
				{
					float x = xOffset + q * width;
					float y = 0.0f;

					Entity entity = CreateNewEntity("Tile");

					MeshComponent mesh = entity.AddComponent<MeshComponent>();
					mesh.SetMesh("Resources/Meshes/hexagon.fbx");

					TransformComponent transform = entity.GetComponent<TransformComponent>();
					transform.Translation = new Vector3(x, z, y);

					m_Tiles.Add(entity);
				}
			}
		}

		void OnUpdate(float timestep)
		{
			
		}
	}
}
