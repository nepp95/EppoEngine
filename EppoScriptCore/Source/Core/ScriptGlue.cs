using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace EppoScriptCore.Core
{
    internal sealed class FieldDescriptor
    {
        public string Name;
        public ScriptFieldType Type;
        public FieldInfo Info;
    }

    internal sealed class MethodDescriptor
    {
        public string Name;
        public MethodInfo Info;
    }

    internal sealed class ClassDescriptor
    {
        public Type Type;
        public string FullName;
        public List<FieldDescriptor> Fields;
        public List<MethodDescriptor> Methods;
    }

    internal sealed class InstanceRecord
    {
        public ScriptBehaviour Instance;
        public ClassDescriptor Descriptor;
    }

    internal sealed class UserAssemblyLoadContext : AssemblyLoadContext
    {
        public UserAssemblyLoadContext() : base(isCollectible: true)
        {
        }

        protected override Assembly? Load(AssemblyName assemblyName)
        {
            var coreAssembly = typeof(ScriptBehaviour).Assembly;
            if (assemblyName.Name == coreAssembly.GetName().Name)
                return coreAssembly;
            return null;
        }
    }

    public static class ScriptGlue
    {
        private static readonly List<ClassDescriptor> s_CoreClasses = new();
        private static readonly List<ClassDescriptor> s_UserClasses = new();
        private static readonly Dictionary<ulong, InstanceRecord> s_Instances = new();
        private static UserAssemblyLoadContext? s_UserContext;

        [UnmanagedCallersOnly(EntryPoint = "Bootstrap")]
        public static void Boostrap()
        {
            s_CoreClasses.Clear();
            s_UserClasses.Clear();
            s_Instances.Clear();

            var assembly = Assembly.GetExecutingAssembly();
            ScanAssembly(assembly, s_CoreClasses);
        }

        private static int TotalClassCount => s_CoreClasses.Count + s_UserClasses.Count;

        private static ClassDescriptor? GetDescriptor(int index)
        {
            if (index < 0)
                return null;

            if (index < s_CoreClasses.Count)
                return s_CoreClasses[index];

            index -= s_CoreClasses.Count;
            if (index < s_UserClasses.Count)
                return s_UserClasses[index];

            return null;
        }

        private static void ScanAssembly(Assembly assembly, List<ClassDescriptor> target)
        {
            foreach (var type in SafeGetTypes(assembly))
            {
                if (type is null || !type.IsPublic || type.IsAbstract)
                    continue;

                if (!type.IsSubclassOf(typeof(ScriptBehaviour)))
                    continue;

                target.Add(BuildDescriptor(type));
            }
        }

        private static ClassDescriptor BuildDescriptor(Type type)
        {
            var descriptor = new ClassDescriptor
            {
                Type = type,
                FullName = type.FullName,
                Fields = [],
                Methods = [],
            };

            foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.Instance))
            {
                if (field.IsInitOnly || field.IsStatic)
                    continue;

                descriptor.Fields.Add(new FieldDescriptor
                {
                    Name = field.Name,
                    Type = ManagedTypeToFieldType(field.FieldType),
                    Info = field,
                });
            }

            foreach (var method in type.GetMethods(BindingFlags.Public | BindingFlags.Instance | BindingFlags.DeclaredOnly))
            {
                if (method.IsSpecialName || method.IsStatic)
                    continue;

                descriptor.Methods.Add(new MethodDescriptor
                {
                    Name = method.Name,
                    Info = method,
                });
            }

            return descriptor;
        }

        [UnmanagedCallersOnly(EntryPoint = "GetClassCount")]
        public static int GetClassCount() => TotalClassCount;

        [UnmanagedCallersOnly(EntryPoint = "GetClassName")]
        public static IntPtr GetClassName(int index)
        {
            var descriptor = GetDescriptor(index);
            if (descriptor is null)
                return IntPtr.Zero;

            return Marshal.StringToCoTaskMemUTF8(descriptor.FullName);
        }

        [UnmanagedCallersOnly(EntryPoint = "GetClassFieldCount")]
        public static int GetClassFieldCount(int classIndex)
        {
            var descriptor = GetDescriptor(classIndex);
            return descriptor?.Fields.Count ?? 0;
        }

        [UnmanagedCallersOnly(EntryPoint = "GetClassFieldName")]
        public static IntPtr GetClassFieldName(int classIndex, int fieldIndex)
        {
            var descriptor = GetDescriptor(classIndex);
            if (descriptor is null)
                return IntPtr.Zero;

            if (fieldIndex < 0 || fieldIndex >= descriptor.Fields.Count)
                return IntPtr.Zero;

            return Marshal.StringToCoTaskMemUTF8(descriptor.Fields[fieldIndex].Name);
        }

        [UnmanagedCallersOnly(EntryPoint = "GetClassFieldType")]
        public static byte GetClassFieldType(int classIndex, int fieldIndex)
        {
            var descriptor = GetDescriptor(classIndex);
            if (descriptor is null)
                return 0;

            if (fieldIndex < 0 || fieldIndex >= descriptor.Fields.Count)
                return 0;

            return (byte)descriptor.Fields[fieldIndex].Type;
        }

        [UnmanagedCallersOnly(EntryPoint = "GetClassMethodCount")]
        public static int GetClassMethodCount(int classIndex)
        {
            var descriptor = GetDescriptor(classIndex);
            return descriptor?.Methods.Count ?? 0;
        }

        [UnmanagedCallersOnly(EntryPoint = "GetClassMethodName")]
        public static IntPtr GetClassMethodName(int classIndex, int methodIndex)
        {
            var descriptor = GetDescriptor(classIndex);
            if (descriptor is null)
                return IntPtr.Zero;

            if (methodIndex < 0 || methodIndex >= descriptor.Methods.Count)
                return IntPtr.Zero;

            return Marshal.StringToCoTaskMemUTF8(descriptor.Methods[methodIndex].Name);
        }

        [UnmanagedCallersOnly(EntryPoint = "LoadUserAssembly")]
        public static void LoadUserAssembly(IntPtr pathPtr)
        {
            var path = Marshal.PtrToStringUTF8(pathPtr);
            if (path is null)
                return;

            UnloadUserAssemblyInternal();
            s_UserContext = new UserAssemblyLoadContext();

            var bytes = File.ReadAllBytes(path);
            using var stream = new MemoryStream(bytes);
            var assembly = s_UserContext.LoadFromStream(stream);

            ScanAssembly(assembly, s_UserClasses);
        }

        [UnmanagedCallersOnly(EntryPoint = "UnloadUserAssembly")]
        public static void UnloadUserAssembly()
        {
            UnloadUserAssemblyInternal();
        }

        private static void UnloadUserAssemblyInternal()
        {
            s_UserClasses.Clear();
            s_Instances.Clear();

            if (s_UserContext is null)
                return;

            var weakRef = new WeakReference(s_UserContext, trackResurrection: true);
            s_UserContext.Unload();
            s_UserContext = null;

            for (var i = 0; weakRef.IsAlive && i < 10; i++)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
        }

        [UnmanagedCallersOnly(EntryPoint = "CreateInstance")]
        public static int CreateInstance(int classIndex, ulong entityId)
        {
            var descriptor = GetDescriptor(classIndex);
            if (descriptor is null)
                return 0;

            if (Activator.CreateInstance(descriptor.Type) is not ScriptBehaviour instance)
                return 0;

            instance.Id = entityId;
            s_Instances[entityId] = new InstanceRecord
            {
                Instance = instance,
                Descriptor = descriptor,
            };

            return 1;
        }

        [UnmanagedCallersOnly(EntryPoint = "DestroyInstance")]
        public static void DestroyInstance(ulong entityId)
        {
            s_Instances.Remove(entityId);
        }

        [UnmanagedCallersOnly(EntryPoint = "InvokeOnCreate")]
        public static void InvokeOnCreate(ulong entityId)
        {
            if (s_Instances.TryGetValue(entityId, out var record))
                record.Instance.OnCreate();
        }

        [UnmanagedCallersOnly(EntryPoint = "InvokeOnUpdate")]
        public static void InvokeOnUpdate(ulong entityId, float timestep)
        {
            if (s_Instances.TryGetValue(entityId, out var record))
                record.Instance.OnUpdate(timestep);
        }

        [UnmanagedCallersOnly(EntryPoint = "InvokeOnDestroy")]
        public static void InvokeOnDestroy(ulong entityId)
        {
            if (s_Instances.TryGetValue(entityId, out var record))
                record.Instance.OnDestroy();
        }

        [UnmanagedCallersOnly(EntryPoint = "SetFieldValue")]
        public static void SetFieldValue(ulong entityId, int fieldIndex, IntPtr data)
        {
            if (data == IntPtr.Zero)
                return;

            if (!s_Instances.TryGetValue(entityId, out var record))
                return;

            var fields = record.Descriptor.Fields;
            if (fieldIndex < 0 || fieldIndex >= fields.Count)
                return;

            var field = fields[fieldIndex];
            var value = ReadFieldValue(data, field.Type);
            if (value is not null)
                field.Info.SetValue(record.Instance, value);
        }

        [UnmanagedCallersOnly(EntryPoint = "GetFieldValue")]
        public static void GetFieldValue(ulong entityId, int fieldIndex, IntPtr data)
        {
            if (data == IntPtr.Zero)
                return;

            if (!s_Instances.TryGetValue(entityId, out var record))
                return;

            var fields = record.Descriptor.Fields;
            if (fieldIndex < 0 || fieldIndex >= fields.Count)
                return;

            var field = fields[fieldIndex];
            var value = field.Info.GetValue(record.Instance);
            if (value is not null)
                WriteFieldValue(data, field.Type, value);
        }

        [UnmanagedCallersOnly(EntryPoint = "FreeString")]
        public static void FreeString(IntPtr ptr) => Marshal.FreeCoTaskMem(ptr);

        [UnmanagedCallersOnly(EntryPoint = "RegisterNativeCallbacks")]
        public static void RegisterNativeCallbacks(IntPtr callbacks) => NativeCallbacks.Register(callbacks);

        private static object? ReadFieldValue(IntPtr data, ScriptFieldType type)
        {
            switch (type)
            {
                case ScriptFieldType.Float: return Marshal.PtrToStructure<float>(data);
                case ScriptFieldType.Double: return Marshal.PtrToStructure<double>(data);
                case ScriptFieldType.Bool: return Marshal.ReadByte(data) != 0;
                case ScriptFieldType.Char: return (char)Marshal.ReadInt16(data);
                case ScriptFieldType.Int16: return Marshal.ReadInt16(data);
                case ScriptFieldType.Int32: return Marshal.ReadInt32(data);
                case ScriptFieldType.Int64: return Marshal.ReadInt64(data);
                case ScriptFieldType.Byte: return Marshal.ReadByte(data);
                case ScriptFieldType.UInt16: return (ushort)Marshal.ReadInt16(data);
                case ScriptFieldType.UInt32: return (uint)Marshal.ReadInt32(data);
                case ScriptFieldType.UInt64: return (ulong)Marshal.ReadInt64(data);
                case ScriptFieldType.Vector2: return Marshal.PtrToStructure<Math.Vector2>(data);
                case ScriptFieldType.Vector3: return Marshal.PtrToStructure<Math.Vector3>(data);
                case ScriptFieldType.Vector4: return Marshal.PtrToStructure<Math.Vector4>(data);
                case ScriptFieldType.Entity: return Marshal.PtrToStructure<Scene.Entity>(data);
                default: return null;
            }
        }

        private static void WriteFieldValue(IntPtr data, ScriptFieldType type, object value)
        {
            switch (type)
            {
                case ScriptFieldType.Float: Marshal.StructureToPtr((float)value, data, false); break;
                case ScriptFieldType.Double: Marshal.StructureToPtr((double)value, data, false); break;
                case ScriptFieldType.Bool: Marshal.WriteByte(data, (byte)((bool)value ? 1 : 0)); break;
                case ScriptFieldType.Char: Marshal.WriteInt16(data, (short)(char)value); break;
                case ScriptFieldType.Int16: Marshal.WriteInt16(data, (short)value); break;
                case ScriptFieldType.Int32: Marshal.WriteInt32(data, (int)value); break;
                case ScriptFieldType.Int64: Marshal.WriteInt64(data, (long)value); break;
                case ScriptFieldType.Byte: Marshal.WriteByte(data, (byte)value); break;
                case ScriptFieldType.UInt16: Marshal.WriteInt16(data, (short)(ushort)value); break;
                case ScriptFieldType.UInt32: Marshal.WriteInt32(data, (int)(uint)value); break;
                case ScriptFieldType.UInt64: Marshal.WriteInt64(data, (long)(ulong)value); break;
                case ScriptFieldType.Vector2: Marshal.StructureToPtr((Math.Vector2)value, data, false); break;
                case ScriptFieldType.Vector3: Marshal.StructureToPtr((Math.Vector3)value, data, false); break;
                case ScriptFieldType.Vector4: Marshal.StructureToPtr((Math.Vector4)value, data, false); break;
                case ScriptFieldType.Entity: Marshal.StructureToPtr((Scene.Entity)value, data, false); break;
                default: break;
            }
        }

        private static IEnumerable<Type?> SafeGetTypes(Assembly assembly)
        {
            try
            {
                return assembly.GetTypes();
            }
            catch (ReflectionTypeLoadException exception)
            {
                return exception.Types.Where(t => t != null);
            }
        }

        private static ScriptFieldType ManagedTypeToFieldType(Type type)
        {
            if (type == typeof(float)) return ScriptFieldType.Float;
            if (type == typeof(double)) return ScriptFieldType.Double;
            if (type == typeof(bool)) return ScriptFieldType.Bool;
            if (type == typeof(char)) return ScriptFieldType.Char;
            if (type == typeof(short)) return ScriptFieldType.Int16;
            if (type == typeof(int)) return ScriptFieldType.Int32;
            if (type == typeof(long)) return ScriptFieldType.Int64;
            if (type == typeof(byte)) return ScriptFieldType.Byte;
            if (type == typeof(ushort)) return ScriptFieldType.UInt16;
            if (type == typeof(uint)) return ScriptFieldType.UInt32;
            if (type == typeof(ulong)) return ScriptFieldType.UInt64;

            return ScriptFieldType.None;
        }
    }
}