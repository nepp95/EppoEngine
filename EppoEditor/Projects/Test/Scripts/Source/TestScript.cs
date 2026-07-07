using EppoScriptCore;
using EppoScriptCore.Core;
using EppoScriptCore.ECS;
using EppoScriptCore.Math;

namespace Test
{
    public class TestScript : ScriptBehaviour
    {
        // All supported field types for discovery testing
        public int IntField = 42;
        public float FloatField = 3.14f;
        public bool BoolField = true;
        public double DoubleField = 2.71828;
        public Entity EntityField;
        public char CharField = 'A';
        public short Int16Field = -32768;
        public long Int64Field = 9223372036854775807;
        public byte ByteField = 255;
        public ushort UInt16Field = 65535;
        public uint UInt32Field = 4294967295;
        public ulong UInt64Field = 18446744073709551615;
        public Vector2 Vector2Field = new(1.0f, 2.0f);
        public Vector3 Vector3Field = new(1.0f, 2.0f, 3.0f);
        public Vector4 Vector4Field = new(1.0f, 2.0f, 3.0f, 4.0f);

        // Methods for discovery testing
        public void MethodA() { }
        public int MethodB() => 42;
        public void MethodWithParam(string message)
        {
            Log.Info(message);
        }

        public override void OnCreate()
        {
            Log.Info("TestScript::OnCreate - IntField = " + IntField);
        }

        public override void OnUpdate(float deltaTime)
        {
            // Test update path
        }

        public override void OnDestroy()
        {
            Log.Info("TestScript::OnDestroy");
        }
    }
}
