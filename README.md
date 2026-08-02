## Install

**Linux:**

*Install Prerequisites (if needed):*
- VulkanSDK
- vcpkg
    - `git clone https://github.com/microsoft/vcpkg.git`
    - `cd vcpkg && ./bootstrap-vcpkg.sh`
    - Set `VCPKG_ROOT` environment variable to root path and add this to your path
- packages
    - `sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config`

*Instructions:*
- `cmake --preset=debug`

## Editor

Run the editor with `EppoEditor/` as its working directory so it reads `Resources/` and `Projects/` directly from the source tree:

```bash
cd EppoEditor
../build/debug/EppoEditor/EppoEditor "path/to/MyProject.epproj"
```

Without an explicit project path, the editor opens `Projects/Test/Test.epproj`.
