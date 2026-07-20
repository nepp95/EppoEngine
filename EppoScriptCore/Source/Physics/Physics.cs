using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace EppoScriptCore.Physics
{
    public struct RaycastHit
    {
        public bool Hit;
        public Vector3 Point;
        public Vector3 Normal;
        public float Distance;
        public Entity? Entity;
    }

    public static class Physics
    {
        public static void ApplyLinearImpulse(Entity entity, Vector3 impulse)
        {
            if (entity.HasComponent<RigidBodyComponent>())
                InternalCalls.Physics_ApplyLinearImpulse(entity.ID, ref impulse);
        }

        public static Vector3 GetLinearVelocity(Entity entity)
        {
            if (entity.HasComponent<RigidBodyComponent>())
            {
                Vector3 result;
                InternalCalls.Physics_GetLinearVelocity(entity.ID, out result);
                return result;
            }

            return Vector3.Zero;
        }

        public static void SetLinearVelocity(Entity entity, Vector3 velocity)
        {
            if (entity.HasComponent<RigidBodyComponent>())
                InternalCalls.Physics_SetLinearVelocity(entity.ID, ref velocity);
        }

        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance, out RaycastHit hit)
        {
            InternalCalls.Physics_Raycast(ref origin, ref direction, maxDistance, out var native);
            hit = new RaycastHit
            {
                Hit = native.Hit != 0,
                Point = native.Point,
                Normal = native.Normal,
                Distance = native.Distance,
                Entity = native.EntityId != 0 ? new Entity(native.EntityId) : null
            };
            return hit.Hit;
        }

        // True when the given entity's physics body overlaps the sphere region.
        public static bool OverlapsSphere(Entity entity, Vector3 center, float radius)
        {
            return InternalCalls.Physics_OverlapsSphere(entity.ID, ref center, radius);
        }
    }
}
