namespace EppoScriptCore.Scene
{
    public struct Entity
    {
        public ulong Id { get; internal set; }

        public Entity(ulong id) => Id = id;
        public static Entity Null => new(0);

        public readonly bool IsValid => Id != 0;
        public override readonly string ToString() => $"Entity({Id})";

        public static bool operator ==(Entity lhs, Entity rhs) => lhs.Id == rhs.Id;
        public static bool operator !=(Entity lhs, Entity rhs) => lhs.Id != rhs.Id;
        public override bool Equals(object? obj) => obj is Entity other && Id == other.Id;
        public override int GetHashCode() => Id.GetHashCode();
    }
}
