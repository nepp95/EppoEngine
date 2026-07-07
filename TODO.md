Thumbnail Rework
I want to create custom thumbnails for all assets we currently support. The way I want it is to have the thumbnail reflect the actual asset. If it is a mesh, show a mesh. If it is a scene, show the scene, etc.
It is important that all the assets are uniformly represented. Same camera angle for example, same background. It could be lit.

Script <-> entity interop
Right now C# scripts can only log and read the keyboard - they can't actually move anything. Wire up Transform access from managed code: Get/SetTranslation (then rotation/scale) callbacks in EppoScriptCore's NativeCallbacks, registered in ScriptEngine::Init, exposed as a Transform property on the managed ScriptBehaviour. This is what makes gameplay scripting actually do something. Cover it in the Scripting test suite.

Viewport gizmos
Editing transforms by typing numbers is painful. Add ImGuizmo (vcpkg - note it churns the CI binary-cache key once) and draw move/rotate/scale handles over the viewport image, operating on the selected entity's TransformComponent. W/E/R to switch mode. Single biggest editor-usability jump.

Entity parenting
The Scene Hierarchy panel implies a hierarchy but the scene is actually flat. Add a RelationshipComponent (parent + children UUIDs), compose world transforms down the tree in Scene/SceneRenderer, and support drag-drop reparenting in the hierarchy panel. Also prerequisite work for prefabs. Cover in the Scene suite.

3D physics (Jolt)
Make play mode an actual game loop: RigidBodyComponent + box/sphere colliders, create bodies on OnScenePlay, step in Scene::OnUpdate and write transforms back, destroy on stop. Jolt is the modern default and is in vcpkg. Biggest item - do it after script interop (so scripts can react to physics) and parenting (so transforms compose).

