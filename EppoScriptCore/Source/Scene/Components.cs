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
}