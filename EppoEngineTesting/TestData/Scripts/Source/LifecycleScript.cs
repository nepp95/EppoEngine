using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace EppoTesting
{
    // Both hooks record what they observed as native side effects rather than
    // fields: the managed instance dies as OnDestroy returns, so the C++ side can
    // only read the scene afterwards.
    public class LifecycleScript : Entity
    {
        public static readonly Vector3 CreateTranslation = new Vector3(1.0f, 2.0f, 3.0f);

        public override void OnCreate()
        {
            Translation = CreateTranslation;
            Scene.CreateEntity("CreatedFromOnCreate");
            Physics.ApplyLinearImpulse(this, new Vector3(0.0f, 5.0f, 0.0f));
        }

        // The written velocity distinguishes three outcomes: non-zero y means physics
        // was still live, zero means the scene resolved but physics was already gone,
        // and OnCreate's translation still in place means nothing resolved.
        public override void OnDestroy()
        {
            Translation = Physics.GetLinearVelocity(this);
            Scene.CreateEntity("CreatedFromOnDestroy");
        }
    }
}
