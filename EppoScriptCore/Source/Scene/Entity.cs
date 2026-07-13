using EppoScriptCore.Core;
using EppoScriptCore.Math;

namespace EppoScriptCore.Scene
{
    // The base class every user script inherits: it is both the live handle to a
    // native entity (id + component access) and the behaviour host (lifecycle
    // virtuals). The runtime instantiates a subclass via the parameterless ctor
    // and assigns ID before OnCreate; component/field marshalling constructs bare
    // handles via Entity(ulong id).
    public class Entity
    {
        // Settable only within this assembly: the runtime writes the owning
        // entity's id after construction, user scripts read it.
        public ulong ID { get; internal set; }
        public string Name => InternalCalls.Entity_GetName(ID);

        // For the runtime's Activator.CreateInstance on user script subclasses;
        // ID is assigned immediately after, before any lifecycle call.
        protected Entity()
        {
        }

        public Entity(ulong id)
        {
            ID = id;
        }

        public virtual void OnCreate()
        {
        }

        public virtual void OnUpdate(float timestep)
        {
        }

        public virtual void OnDestroy()
        {
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

        public static bool operator ==(Entity? lhs, Entity? rhs)
        {
            if (ReferenceEquals(lhs, rhs))
                return true;
            if (lhs is null || rhs is null)
                return false;
            return lhs.ID == rhs.ID;
        }

        public static bool operator !=(Entity? lhs, Entity? rhs) => !(lhs == rhs);
        public override bool Equals(object? obj) => obj is Entity other && ID == other.ID;
        public override int GetHashCode() => ID.GetHashCode();
        public override string ToString() => $"Entity({ID})";
    }
}
