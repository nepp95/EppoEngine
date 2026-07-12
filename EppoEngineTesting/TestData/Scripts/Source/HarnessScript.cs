using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace EppoTesting
{
    // A minimal concrete script the Scripting suite asserts on. Kept small and
    // stable — the C++ tests depend on these names/types.
    public class HarnessScript : ScriptBehaviour
    {
        public float Speed = 2.5f;
        public int Count = 7;

        // Extra field types the marshalling round-trip tests assert on.
        public bool Enabled = true;
        public double Ratio = 1.5;
        public Vector3 Position;
        public Entity? Target;

        // Observable lifecycle side effects so the C++ tests can prove OnCreate /
        // OnUpdate actually route through managed and back via field reads.
        public int Created;
        public float Accumulated;

        public override void OnCreate() => Created = 1;
        public override void OnUpdate(float deltaTime) => Accumulated += deltaTime;

        // Exercised by the GetMethod/InvokeMethod round-trip.
        public int Add(int a, int b) => a + b;

        // Proves native callbacks route back into the engine: logs via the Log
        // internal call and returns the Input internal call's result.
        public int Probe()
        {
            Log.Info("HarnessScript.Probe: native Log callback reached from script");
            return Input.IsKeyPressed(KeyCode.Space) ? 1 : 0;
        }

        // Exercised by ScriptExceptionDoesNotCrashHost: proves an exception thrown
        // from user code doesn't escape the UnmanagedCallersOnly boundary and kill the host.
        public int Throws() => throw new System.InvalidOperationException("boom from script");
    }
}
