#pragma once

#include "Core/Base.h"

#include "Asset/Asset.h"
#include "Asset/AssetMetadata.h"
#include "Asset/AssetType.h"

#include "Core/Application.h"
#include "Core/Buffer/Buffer.h"
#include "Core/Buffer/BufferReader.h"
#include "Core/Buffer/BufferWriter.h"
#include "Core/Buffer/FileStreamReader.h"
#include "Core/Buffer/FileStreamWriter.h"
#include "Core/ThreadPool/ThreadPool.h"
#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/Layer.h"
#include "Core/MouseCodes.h"
#include "Core/Timer.h"

#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"

#include "ImGui/FileDialog.h"
#include "ImGui/ImExt.h"
#include "ImGui/ScopedBegin.h"

#include <ImGuizmo.h>

#include "Project/GameData.h"
#include "Project/Project.h"
#include "Project/ProjectExporter.h"
#include "Project/ProjectSerializer.h"

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Camera/SceneCamera.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"

#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include "Scripting/ScriptEngine.h"

#include "Utility/ErrorDialog.h"
#include "Utility/Filesystem.h"
#include "Utility/FileWatcher.h"
#include "Utility/Json.h"
#include "Utility/Random.h"
