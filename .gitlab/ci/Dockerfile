# EppoEngine CI toolchain image (Linux)
# =============================================================================
# Everything the pipeline needs to configure, build and test the engine, baked
# into one image that lives in the project Container Registry:
#   * clang / lld            — the linux-* CMake presets use clang/clang++
#   * CMake >= 3.31 + Ninja  — required by CMakePresets.json
#   * vcpkg (VCPKG_ROOT)     — dependency manager (manifest = vcpkg.json)
#   * Vulkan SDK (VULKAN_SDK)— provides the loader + dxc (find_package COMPONENTS dxc)
#   * .NET SDK 10 (DOTNET_ROOT) — hosted runtime for the C# scripting suite (net10.0)
#   * X11/Wayland/GL dev libs — GLFW (pulled in by imgui) links against them
#
# The vcpkg *dependencies* are NOT baked in here — they are restored from the
# GitLab job cache instead (see .gitlab-ci.yml), so a manifest bump doesn't force
# an image rebuild. This image is rebuilt only when this Dockerfile changes.
# =============================================================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# --- Base toolchain + build/system deps --------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget git \
        clang lld llvm \
        ninja-build make pkg-config \
        zip unzip tar xz-utils \
        python3 \
        autoconf automake libtool bison flex \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
        libxext-dev libwayland-dev libxkbcommon-dev wayland-protocols \
        libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

# --- CMake >= 3.31 (Ubuntu 24.04 ships 3.28, too old for the presets) ---------
ARG CMAKE_VERSION=3.31.6
RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh" \
        -o /tmp/cmake.sh \
    && sh /tmp/cmake.sh --skip-license --prefix=/usr/local \
    && rm /tmp/cmake.sh \
    && cmake --version

# --- Vulkan SDK (loader + dxc), installed from the LunarG tarball -------------
# The tarball (unlike the apt meta-package) reliably ships dxc, which
# find_package(Vulkan REQUIRED COMPONENTS dxc) needs.
ARG VULKAN_SDK_VERSION=1.4.309.0
RUN curl -fsSL "https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz" \
        -o /tmp/vulkansdk.tar.xz \
    && mkdir -p /opt/vulkan \
    && tar -xf /tmp/vulkansdk.tar.xz -C /opt/vulkan \
    && rm /tmp/vulkansdk.tar.xz
ENV VULKAN_SDK=/opt/vulkan/${VULKAN_SDK_VERSION}/x86_64
ENV PATH="${VULKAN_SDK}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${VULKAN_SDK}/lib"
ENV CMAKE_PREFIX_PATH="${VULKAN_SDK}"

# --- .NET SDK 10 (scripting runtime is net10.0) ------------------------------
ENV DOTNET_ROOT=/usr/share/dotnet
RUN curl -fsSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh \
    && bash /tmp/dotnet-install.sh --channel 10.0 --install-dir "${DOTNET_ROOT}" \
    && rm /tmp/dotnet-install.sh
ENV PATH="${DOTNET_ROOT}:${PATH}"
ENV DOTNET_CLI_TELEMETRY_OPTOUT=1 \
    DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 \
    DOTNET_NOLOGO=1

# --- vcpkg (full clone so the manifest's builtin-baseline commit is present) --
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg "${VCPKG_ROOT}" \
    && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
ENV PATH="${VCPKG_ROOT}:${PATH}" \
    VCPKG_DISABLE_METRICS=1

# clang-scan-deps: CMake >= 3.28 scans C++20 sources for modules under Ninja+Clang;
# without it the build fails with CLANG_SCAN_DEPS-NOTFOUND (exit 127).
# autoconf-archive: required by the libxcrypt vcpkg port's autoreconf step.
RUN apt-get update && apt-get install -y --no-install-recommends \
        clang-tools-18 autoconf-archive libltdl-dev \
        python3-venv python3-pip \
    && rm -rf /var/lib/apt/lists/* \
    && ln -sf /usr/bin/clang-scan-deps-18 /usr/bin/clang-scan-deps

# Default clang toolchain for the linux-* presets.
ENV CC=clang CXX=clang++
