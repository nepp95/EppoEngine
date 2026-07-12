using EppoScriptCore.Core;
using EppoScriptCore.Math;

namespace EppoScriptCore.Scene
{
    public class Entity
    {
        public readonly ulong ID;
        public string Name { get; }

        public Entity(ulong id)
        {
            ID = id;
        }

        // Convenience shortcut to the entity's TransformComponent translation;
        // routes through the same internal calls as TransformComponent.Translation.
        public Vector3 Translation
        {
            get => InternalCalls.TransformComponent_GetTranslation(ID);
            set => InternalCalls.TransformComponent_SetTranslation(ID, ref value);
        }

        public bool HasComponent<T>() where T : Component, new()
            => InternalCalls.Entity_HasComponent(ID, typeof(T).Name);

        public T AddComponent<T>() where T : Component, new()
        {
            if (HasComponent<T>())
                return GetComponent<T>();

            InternalCalls.Entity_AddComponent(ID, typeof(T).Name);
            return new T { Entity = this };
        }

        public T GetComponent<T>() where T : Component, new()
        {
            if (!HasComponent<T>())
                return null;

            return new T { Entity = this };
        }

        public bool RemoveComponent<T>() where T : Component, new()
            => InternalCalls.Entity_RemoveComponent(ID, typeof(T).Name);

        public override string ToString() => $"Entity({ID})";
        public static bool operator ==(Entity lhs, Entity rhs) => lhs.ID == rhs.ID;
        public static bool operator !=(Entity lhs, Entity rhs) => lhs.ID != rhs.ID;
        public override bool Equals(object? obj) => obj is Entity other && ID == other.ID;
        public override int GetHashCode() => ID.GetHashCode();
    }
}
