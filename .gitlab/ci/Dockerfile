FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Ubuntu distributes CTest in the cmake package; Eppo never invokes CMake to generate or build.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git wget \
        clang lld llvm \
        cmake ninja-build make pkg-config uuid-dev \
        zip unzip tar xz-utils \
        python3 \
        autoconf automake autoconf-archive libtool libltdl-dev bison flex \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
        libxext-dev libwayland-dev libxkbcommon-dev wayland-protocols \
        libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

ARG PREMAKE_VERSION=5.0.0-beta8
ARG PREMAKE_COMMIT=2ca338a25ed5f6e62d36c9cd70e5f313953c630d
RUN git clone --branch "v${PREMAKE_VERSION}" --depth 1 https://github.com/premake/premake-core.git /tmp/premake \
    && test "$(git -C /tmp/premake rev-parse HEAD)" = "${PREMAKE_COMMIT}" \
    && cd /tmp/premake \
    && CC=clang CXX=clang++ PREMAKE_OPTS=--cc=clang sh Bootstrap.sh \
    && install -m 0755 bin/release/premake5 /usr/local/bin/premake5 \
    && rm -rf /tmp/premake \
    && premake5 --version | grep "${PREMAKE_VERSION}"

ARG VULKAN_SDK_VERSION=1.4.309.0
RUN curl -fsSL "https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz" \
        -o /tmp/vulkansdk.tar.xz \
    && mkdir -p /opt/vulkan \
    && tar -xf /tmp/vulkansdk.tar.xz -C /opt/vulkan \
    && rm /tmp/vulkansdk.tar.xz
ENV VULKAN_SDK=/opt/vulkan/${VULKAN_SDK_VERSION}/x86_64
ENV PATH="${VULKAN_SDK}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${VULKAN_SDK}/lib"

ENV DOTNET_ROOT=/usr/share/dotnet
RUN curl -fsSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh \
    && bash /tmp/dotnet-install.sh --channel 10.0 --install-dir "${DOTNET_ROOT}" \
    && rm /tmp/dotnet-install.sh
ENV PATH="${DOTNET_ROOT}:${PATH}"
ENV DOTNET_CLI_TELEMETRY_OPTOUT=1 \
    DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 \
    DOTNET_NOLOGO=1

ENV VCPKG_ROOT=/opt/vcpkg
ARG VCPKG_COMMIT=8779f47d2db0785ee034b47fdfdcdb0e88b7010e
RUN git init "${VCPKG_ROOT}" \
    && git -C "${VCPKG_ROOT}" remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C "${VCPKG_ROOT}" fetch --depth 1 origin "${VCPKG_COMMIT}" \
    && git -C "${VCPKG_ROOT}" checkout --detach FETCH_HEAD \
    && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
ENV PATH="${VCPKG_ROOT}:${PATH}" \
    VCPKG_DISABLE_METRICS=1 \
    CC=clang \
    CXX=clang++
