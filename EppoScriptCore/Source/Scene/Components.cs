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

        public Vector3 Rotation
        {
            get => InternalCalls.TransformComponent_GetRotation(Entity.ID);
            set => InternalCalls.TransformComponent_SetRotation(Entity.ID, ref value);
        }

        public Vector3 Scale
        {
            get => InternalCalls.TransformComponent_GetScale(Entity.ID);
            set => InternalCalls.TransformComponent_SetScale(Entity.ID, ref value);
        }
    }

    public class MeshComponent : Component
    {
        // A handle that isn't a mesh is rejected natively; the component keeps its
        // previous value.
        public ulong MeshHandle
        {
            get => InternalCalls.MeshComponent_GetMeshHandle(Entity.ID);
            set => InternalCalls.MeshComponent_SetMeshHandle(Entity.ID, value);
        }

        // Preferred over assigning MeshHandle directly: needs no knowledge of the
        // project's asset registry.
        public void SetPrimitive(PrimitiveMesh primitive)
            => MeshHandle = (ulong)primitive;
    }

    public class CameraComponent : Component
    {
        public bool Primary
        {
            get => InternalCalls.CameraComponent_GetPrimary(Entity.ID);
            set => InternalCalls.CameraComponent_SetPrimary(Entity.ID, value);
        }

        public float VerticalFov
        {
            get => InternalCalls.CameraComponent_GetVerticalFov(Entity.ID);
            set => InternalCalls.CameraComponent_SetVerticalFov(Entity.ID, value);
        }

        public float NearClip
        {
            get => InternalCalls.CameraComponent_GetNearClip(Entity.ID);
            set => InternalCalls.CameraComponent_SetNearClip(Entity.ID, value);
        }

        public float FarClip
        {
            get => InternalCalls.CameraComponent_GetFarClip(Entity.ID);
            set => InternalCalls.CameraComponent_SetFarClip(Entity.ID, value);
        }
    }

    public class DirectionalLightComponent : Component
    {
        public Vector3 Color
        {
            get => InternalCalls.DirectionalLightComponent_GetColor(Entity.ID);
            set => InternalCalls.DirectionalLightComponent_SetColor(Entity.ID, ref value);
        }

        public float Intensity
        {
            get => InternalCalls.DirectionalLightComponent_GetIntensity(Entity.ID);
            set => InternalCalls.DirectionalLightComponent_SetIntensity(Entity.ID, value);
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

        public ulong[] Children => InternalCalls.RelationshipComponent_GetChildren(Entity.ID);
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

        public float GravityScale
        {
            get => InternalCalls.RigidBodyComponent_GetGravityScale(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetGravityScale(Entity.ID, value);
        }

        public float LinearDamping
        {
            get => InternalCalls.RigidBodyComponent_GetLinearDamping(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLinearDamping(Entity.ID, value);
        }

        public float AngularDamping
        {
            get => InternalCalls.RigidBodyComponent_GetAngularDamping(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetAngularDamping(Entity.ID, value);
        }

        public bool LockLinearX
        {
            get => InternalCalls.RigidBodyComponent_GetLockLinearX(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLockLinearX(Entity.ID, value);
        }

        public bool LockLinearY
        {
            get => InternalCalls.RigidBodyComponent_GetLockLinearY(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLockLinearY(Entity.ID, value);
        }

        public bool LockLinearZ
        {
            get => InternalCalls.RigidBodyComponent_GetLockLinearZ(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLockLinearZ(Entity.ID, value);
        }

        public bool LockAngularX
        {
            get => InternalCalls.RigidBodyComponent_GetLockAngularX(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLockAngularX(Entity.ID, value);
        }

        public bool LockAngularY
        {
            get => InternalCalls.RigidBodyComponent_GetLockAngularY(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLockAngularY(Entity.ID, value);
        }

        public bool LockAngularZ
        {
            get => InternalCalls.RigidBodyComponent_GetLockAngularZ(Entity.ID);
            set => InternalCalls.RigidBodyComponent_SetLockAngularZ(Entity.ID, value);
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
