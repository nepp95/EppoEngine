#pragma once

typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoClass MonoClass;
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoImage MonoImage;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoObject MonoObject;

namespace Eppo
{
	class ScriptClass;

	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static MonoObject* InstantiateClass(MonoClass* monoClass);

		static MonoImage* GetCoreAssemblyImage();
		static MonoImage* GetAppAssemblyImage();

		static Ref<ScriptClass> GetEntityClass();

	private:
		static void InitMono();
		static bool LoadCoreAssembly(const std::filesystem::path& filepath);
		static void LoadAssemblyClasses();
	};
}
