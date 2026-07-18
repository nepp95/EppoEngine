using EppoScriptCore.Core;
using EppoScriptCore.Math;

namespace EppoScriptCore.Scene
{
    public abstract class Component
    {
        public Entity Entity { get; internal set; }
    }

    public class TransformComponent : Component
    {
        public Vector3 Translation
        {
            get => InternalCalls.TransformComponent_GetTranslation(Entity.ID);
            set => InternalCalls.TransformComponent_SetTranslation(Entity.ID, ref value);
        }
    }

    public class MeshComponent : Component
    {
        public ulong MeshHandle => InternalCalls.MeshComponent_GetMeshHandle(Entity.ID);
    }

    public class CameraComponent : Component
    {
        public bool Primary
        {
            get => InternalCalls.CameraComponent_GetPrimary(Entity.ID);
            set => InternalCalls.CameraComponent_SetPrimary(Entity.ID, value);
        }
    }

    public class PointLightComponent : Component
    {
        public Vector3 Color
        {
            get => InternalCalls.PointLightComponent_GetColor(Entity.ID);
            set => InternalCalls.PointLightComponent_SetColor(Entity.ID, ref value);
        }

        public float Intensity
        {
            get => InternalCalls.PointLightComponent_GetIntensity(Entity.ID);
            set => InternalCalls.PointLightComponent_SetIntensity(Entity.ID, value);
        }
    }

    public class RelationshipComponent : Component
    {
        public ulong Parent
        {
            get => InternalCalls.RelationshipComponent_GetParent(Entity.ID);
            set => InternalCalls.RelationshipComponent_SetParent(Entity.ID, value);
        }
    }

    public class RigidBodyComponent : Component
    {
        public enum BodyType : byte { Static = 0, Kinematic, Dynamic };

        public BodyType Type
        {
            get => (BodyType)InternalCalls.RigidBodyComponent_GetType(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetType(Entity.ID, (byte)value);
        }

        public Vector3 LinearVelocity
        {
            get
            {
                InternalCalls.Physics_GetLinearVelocity(Entity.ID, out Vector3 velocity);
                return velocity;
            }
            set => InternalCalls.Physics_SetLinearVelocity(Entity.ID, ref value);
        }
    }

    public class BoxColliderComponent : Component
    {
        public Vector3 HalfSize
        {
            get => InternalCalls.BoxColliderComponent_GetHalfSize(Entity.ID);
            set => InternalCalls.BoxColliderComponent_SetHalfSize(Entity.ID, ref value);
        }

        public Vector3 Offset
        {
            get => InternalCalls.BoxColliderComponent_GetOffset(Entity.ID);
            set => InternalCalls.BoxColliderComponent_SetOffset(Entity.ID, ref value);
        }

        public float Density
        {
            get => InternalCalls.BoxColliderComponent_GetDensity(Entity.ID);
            set => InternalCalls.BoxColliderComponent_SetDensity(Entity.ID, value);
        }
        
        public float Friction
        {
            get => InternalCalls.BoxColliderComponent_GetFriction(Entity.ID);
            set => InternalCalls.BoxColliderComponent_SetFriction(Entity.ID, value);
        }

        public float Restitution
        {
            get => InternalCalls.BoxColliderComponent_GetRestitution(Entity.ID);
            set => InternalCalls.BoxColliderComponent_SetRestitution(Entity.ID, value);
        }
    }

    public class SphereColliderComponent : Component
    {
        public float Radius
        {
            get => InternalCalls.SphereColliderComponent_GetRadius(Entity.ID);
            set => InternalCalls.SphereColliderComponent_SetRadius(Entity.ID, value);
        }

        public Vector3 Offset
        {
            get => InternalCalls.SphereColliderComponent_GetOffset(Entity.ID);
            set => InternalCalls.SphereColliderComponent_SetOffset(Entity.ID, ref value);
        }
        public float Density
        {
            get => InternalCalls.SphereColliderComponent_GetDensity(Entity.ID);
            set => InternalCalls.SphereColliderComponent_SetDensity(Entity.ID, value);
        }

        public float Friction
        {
            get => InternalCalls.SphereColliderComponent_GetFriction(Entity.ID);
            set => InternalCalls.SphereColliderComponent_SetFriction(Entity.ID, value);
        }

        public float Restitution
        {
            get => InternalCalls.SphereColliderComponent_GetRestitution(Entity.ID);
            set => InternalCalls.SphereColliderComponent_SetRestitution(Entity.ID, value);
        }
    }

    public class CapsuleColliderComponent : Component
    {
        public float Radius
        {
            get => InternalCalls.CapsuleColliderComponent_GetRadius(Entity.ID);
            set => InternalCalls.CapsuleColliderComponent_SetRadius(Entity.ID, value);
        }

        public float Height
        {
            get => InternalCalls.CapsuleColliderComponent_GetHeight(Entity.ID);
            set => InternalCalls.CapsuleColliderComponent_SetHeight(Entity.ID, value);
        }

        public Vector3 Offset
        {
            get => InternalCalls.CapsuleColliderComponent_GetOffset(Entity.ID);
            set => InternalCalls.CapsuleColliderComponent_SetOffset(Entity.ID, ref value);
        }

        public float Density
        {
            get => InternalCalls.CapsuleColliderComponent_GetDensity(Entity.ID);
            set => InternalCalls.CapsuleColliderComponent_SetDensity(Entity.ID, value);
        }

        public float Friction
        {
            get => InternalCalls.CapsuleColliderComponent_GetFriction(Entity.ID);
            set => InternalCalls.CapsuleColliderComponent_SetFriction(Entity.ID, value);
        }

        public float Restitution
        {
            get => InternalCalls.CapsuleColliderComponent_GetRestitution(Entity.ID);
            set => InternalCalls.CapsuleColliderComponent_SetRestitution(Entity.ID, value);
        }
    }

    public class CylinderColliderComponent : Component
    {
        public float Radius
        {
            get => InternalCalls.CylinderColliderComponent_GetRadius(Entity.ID);
            set => InternalCalls.CylinderColliderComponent_SetRadius(Entity.ID, value);
        }

        public float Height
        {
            get => InternalCalls.CylinderColliderComponent_GetHeight(Entity.ID);
            set => InternalCalls.CylinderColliderComponent_SetHeight(Entity.ID, value);
        }

        public Vector3 Offset
        {
            get => InternalCalls.CylinderColliderComponent_GetOffset(Entity.ID);
            set => InternalCalls.CylinderColliderComponent_SetOffset(Entity.ID, ref value);
        }

        public float Density
        {
            get => InternalCalls.CylinderColliderComponent_GetDensity(Entity.ID);
            set => InternalCalls.CylinderColliderComponent_SetDensity(Entity.ID, value);
        }

        public float Friction
        {
            get => InternalCalls.CylinderColliderComponent_GetFriction(Entity.ID);
            set => InternalCalls.CylinderColliderComponent_SetFriction(Entity.ID, value);
        }

        public float Restitution
        {
            get => InternalCalls.CylinderColliderComponent_GetRestitution(Entity.ID);
            set => InternalCalls.CylinderColliderComponent_SetRestitution(Entity.ID, value);
        }
    }
}
