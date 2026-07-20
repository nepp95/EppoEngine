Thumbnail Rework
I want to create custom thumbnails for all assets we currently support. The way I want it is to have the thumbnail reflect the actual asset. If it is a mesh, show a mesh. If it is a scene, show the scene, etc.
It is important that all the assets are uniformly represented. Same camera angle for example, same background. It could be lit.

Viewport gizmos
Editing transforms by typing numbers is painful. Add ImGuizmo (vcpkg - note it churns the CI binary-cache key once) and draw move/rotate/scale handles over the viewport image, operating on the selected entity's TransformComponent. W/E/R to switch mode. Single biggest editor-usability jump.
