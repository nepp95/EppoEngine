using System;

namespace Eppo
{
	public class Entity
	{
		public readonly ulong ID;
		public string Name => InternalCalls.Entity_GetName(ID);

		protected Entity()
		{
			ID = 0;
		}

		internal Entity(ulong id)
		{
			ID = id;
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;
			if (!(obj is Entity))
				return false;

			return ((Entity)obj).ID == this.ID;
		}

		public override int GetHashCode()
		{
			return ID.GetHashCode();
		}

		public static bool operator==(Entity a, Entity b)
		{
			if (a == null)
				if (b == null)
					return true;
				else
					return false;
			return a.Equals(b);
		}

		public static bool operator!=(Entity a, Entity b)
		{
			return !(a == b);
		}

		public Vector3 Translation
		{
			get
			{
				InternalCalls.TransformComponent_GetTranslation(ID, out Vector3 translation);
				return translation;
			}
			set => InternalCalls.TransformComponent_SetTranslation(ID, ref value);
		}

		public T AddComponent<T>() where T : Component, new()
		{
			if (HasComponent<T>())
				return null;

			T component = new T()
			{
				Entity = this
			};

			InternalCalls.Entity_AddComponent(ID, component.ComponentTypeName);

			return component;
		}

		public bool HasComponent<T>() where T : Component, new()
		{
			Type componentType = typeof(T);
			return InternalCalls.Entity_HasComponent(ID, componentType);
		}

		public T GetComponent<T>() where T : Component, new()
		{
			if (!HasComponent<T>())
				return null;

			T component = new T()
			{ 
				Entity = this 
			};

			return component;
		}

		public T As<T>() where T : Entity, new()
		{
			object instance = InternalCalls.GetScriptInstance(ID);
			return instance as T;
		}

		public static Entity FindEntityByName(string name)
		{
			ulong uuid = InternalCalls.Entity_FindEntityByName(name);
			if (uuid == 0)
				return null;

			return new Entity(uuid);
		}

		public static Entity CreateNewEntity(string name = "")
		{
			ulong uuid = InternalCalls.Entity_CreateNewEntity(name);
			if (uuid == 0)
				return null;

			return new Entity(uuid);
		}
	}
}
