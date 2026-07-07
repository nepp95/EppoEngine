using EppoScriptCore;

namespace EppoTesting
{
    // A minimal concrete script used by the Scripting suite to exercise user
    // assembly loading, class discovery, and public-field reflection. Kept
    // deliberately small and stable — the C++ tests assert on these names/types.
    public class HarnessScript : ScriptBehaviour
    {
        public float Speed = 2.5f;
        public int Count = 7;

        public override void OnCreate()
        {
        }

        public override void OnUpdate(float deltaTime)
        {
        }
    }
}
