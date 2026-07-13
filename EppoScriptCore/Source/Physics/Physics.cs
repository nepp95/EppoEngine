using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace EppoScriptCore.Physics
{
    public class Physics
    {
        public void ApplyLinearImpulse(Entity entity, Vector3 impulse)
        {
            if (entity.HasComponent<RigidBodyComponent>())
                InternalCalls.Physics_ApplyLinearImpulse(entity.ID, ref impulse);
        }

        public Vector3 GetLinearVelocity(Entity entity)
        {
            if (entity.HasComponent<RigidBodyComponent>())
            {
                Vector3 result;
                InternalCalls.Physics_GetLinearVelocity(entity.ID, out result);
                return result;
            }

            return Vector3.Zero;
        }

        public void SetLinearVelocity(Entity entity, Vector3 velocity)
        {
            if (entity.HasComponent<RigidBodyComponent>())
                InternalCalls.Physics_SetLinearVelocity(entity.ID, ref velocity);
        }
    }
}