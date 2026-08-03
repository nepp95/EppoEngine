#!/usr/bin/env python3

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Mapping


PREMAKE_VERSION = "5.0.0-beta8"
PREMAKE_TAG = f"v{PREMAKE_VERSION}"
PREMAKE_COMMIT = "2ca338a25ed5f6e62d36c9cd70e5f313953c630d"
PREMAKE_REPOSITORY = "https://github.com/premake/premake-core.git"
VCPKG_COMMIT = "8779f47d2db0785ee034b47fdfdcdb0e88b7010e"
VCPKG_REPOSITORY = "https://github.com/microsoft/vcpkg.git"
WINDOWS_ACTIONS = ("vs2022", "vs2026")
SUPPORTED_ACTIONS = (*WINDOWS_ACTIONS, "ninja")
CTEST_MINIMUM_VERSION = (3, 21)
NINJA_MINIMUM_VERSION = (1, 6)
NINJA_VERSION = "1.13.1"


class SetupError(RuntimeError):
    pass


def host_system() -> str:
    return platform.system().lower()


def parse_premake_version(output: str) -> str | None:
    match = re.search(r"\b(\d+\.\d+\.\d+-beta\d+)\b", output)
    if not match or match.group(1) != PREMAKE_VERSION:
        return None
    return match.group(1)


def clean_environment(environment: Mapping[str, str]) -> dict[str, str]:
    result: dict[str, str] = {}
    keys: dict[str, str] = {}
    for key, value in environment.items():
        folded = key.casefold()
        previous = keys.get(folded)
        if previous is not None:
            result.pop(previous, None)
        keys[folded] = key
        result[key] = value
    return result


def default_action(system: str) -> str | None:
    return "ninja" if system == "linux" else None


def local_premake_path(root: Path, system: str) -> Path:
    executable = "premake5.exe" if system == "windows" else "premake5"
    return root / ".eppo" / "tools" / "premake" / PREMAKE_VERSION / "bin" / executable


def local_vcpkg_path(root: Path, system: str) -> Path:
    executable = "vcpkg.exe" if system == "windows" else "vcpkg"
    return root / ".eppo" / "tools" / "vcpkg" / VCPKG_COMMIT / executable


def local_ninja_path(root: Path, system: str) -> Path:
    executable = "ninja.exe" if system == "windows" else "ninja"
    return root / ".eppo" / "tools" / "ninja" / NINJA_VERSION / executable


def version_at_least(output: str, minimum: tuple[int, ...]) -> bool:
    match = re.search(r"\b(\d+)\.(\d+)(?:\.(\d+))?\b", output)
    if not match:
        return False
    version = tuple(int(component or 0) for component in match.groups())
    padded_minimum = minimum + (0,) * (len(version) - len(minimum))
    return version >= padded_minimum


def run(command: list[str], *, cwd: Path | None = None, environment: Mapping[str, str] | None = None, capture: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        env=clean_environment(environment or os.environ),
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )


def executable_version(executable: Path, arguments: list[str]) -> str | None:
    try:
        result = run([str(executable), *arguments], capture=True)
    except OSError:
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def find_premake(root: Path, system: str) -> Path | None:
    candidates: list[Path] = []
    system_premake = shutil.which("premake5")
    if system_premake:
        candidates.append(Path(system_premake))
    candidates.append(local_premake_path(root, system))

    for candidate in candidates:
        version = executable_version(candidate, ["--version"])
        if version and parse_premake_version(version):
            return candidate
    return None


def find_vswhere() -> Path | None:
    candidate = shutil.which("vswhere")
    if candidate:
        return Path(candidate)
    program_files = os.environ.get("ProgramFiles(x86)")
    if not program_files:
        return None
    candidate_path = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    return candidate_path if candidate_path.is_file() else None


def find_visual_studio(action: str) -> tuple[Path, Path]:
    vswhere = find_vswhere()
    if not vswhere:
        raise SetupError("Visual Studio Installer's vswhere.exe was not found.")

    version_range = "[17.0,18.0)" if action == "vs2022" else "[18.0,19.0)"
    result = run(
        [
            str(vswhere),
            "-nologo",
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-version",
            version_range,
            "-property",
            "installationPath",
        ],
        capture=True,
    )
    installation = Path(result.stdout.strip()) if result.returncode == 0 and result.stdout.strip() else None
    if not installation:
        raise SetupError(f"A Visual Studio installation matching {action} with the C++ workload was not found.")

    vcvars = installation / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    msbuild = installation / "MSBuild" / "Current" / "Bin" / "MSBuild.exe"
    if not vcvars.is_file() or not msbuild.is_file():
        raise SetupError(f"The Visual Studio installation at '{installation}' is missing vcvars64.bat or MSBuild.exe.")
    return vcvars, msbuild


def find_windows_toolchain(action: str) -> tuple[Path, Path]:
    return find_visual_studio(action)


def premake_bootstrap_command(vcvars: Path, action: str) -> str:
    return (
        f'call "{vcvars}" && '
        f'nmake -f Bootstrap.mak windows-msbuild MSDEV={action} CONFIG=release PLATFORM=x64'
    )


def confirm(message: str, assume_yes: bool) -> bool:
    if assume_yes:
        return True
    if not sys.stdin.isatty():
        return False
    response = input(f"{message} [y/N] ").strip().casefold()
    return response in {"y", "yes"}


def clone_premake(source: Path, offline: bool) -> None:
    if source.is_dir():
        result = run(["git", "-C", str(source), "rev-parse", "HEAD"], capture=True)
        if result.returncode == 0 and result.stdout.strip() == PREMAKE_COMMIT:
            return
        raise SetupError(f"The existing Premake source at '{source}' is not the pinned commit. Remove it and rerun setup.")
    if offline:
        raise SetupError("Premake is missing and offline mode prevents cloning its source.")

    source.parent.mkdir(parents=True, exist_ok=True)
    result = run(["git", "clone", "--branch", PREMAKE_TAG, "--depth", "1", PREMAKE_REPOSITORY, str(source)])
    if result.returncode != 0:
        raise SetupError("Cloning Premake source failed.")
    result = run(["git", "-C", str(source), "rev-parse", "HEAD"], capture=True)
    if result.returncode != 0 or result.stdout.strip() != PREMAKE_COMMIT:
        raise SetupError("The cloned Premake source does not match the pinned beta8 commit.")


def build_premake(root: Path, system: str, action: str, assume_yes: bool, offline: bool) -> Path:
    if not confirm(f"Premake {PREMAKE_VERSION} is missing. Clone and build it from source in '{root / '.eppo' / 'tools'}'?", assume_yes):
        raise SetupError("Premake provisioning was declined.")

    tool_root = root / ".eppo" / "tools" / "premake" / PREMAKE_VERSION
    source = tool_root / "source"
    destination = local_premake_path(root, system)
    clone_premake(source, offline)

    if system == "windows":
        vcvars, _ = find_visual_studio(action)
        command = premake_bootstrap_command(vcvars, action)
        bootstrap_script = tool_root / "bootstrap.cmd"
        bootstrap_script.write_text(f"@ECHO OFF\n{command}\n", encoding="utf-8")
        result = run(["cmd.exe", "/d", "/c", str(bootstrap_script)], cwd=source)
        built = source / "bin" / "release" / "premake5.exe"
    elif system == "linux":
        environment = clean_environment(os.environ)
        environment["CC"] = "clang"
        environment["CXX"] = "clang++"
        environment["CONFIG"] = "release"
        environment["PREMAKE_OPTS"] = "--cc=clang"
        result = run(["sh", "Bootstrap.sh"], cwd=source, environment=environment)
        built = source / "bin" / "release" / "premake5"
    else:
        raise SetupError(f"Unsupported host platform '{system}'.")

    if result.returncode != 0 or not built.is_file():
        raise SetupError("Building Premake from source failed.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, destination)
    if system != "windows":
        destination.chmod(0o755)
    version = executable_version(destination, ["--version"])
    if not version or not parse_premake_version(version):
        raise SetupError("The source-built Premake executable reports an unexpected version.")
    return destination


def find_vcpkg(root: Path, system: str) -> tuple[Path, Path] | None:
    executable_name = "vcpkg.exe" if system == "windows" else "vcpkg"
    candidates: list[Path] = []
    environment_root = os.environ.get("VCPKG_ROOT")
    if environment_root:
        candidates.append(Path(environment_root) / executable_name)
    system_vcpkg = shutil.which("vcpkg")
    if system_vcpkg:
        candidates.append(Path(system_vcpkg))
    candidates.append(local_vcpkg_path(root, system))

    for candidate in candidates:
        if candidate.is_file():
            return candidate, candidate.parent
    return None


def provision_vcpkg(root: Path, system: str, assume_yes: bool, offline: bool) -> tuple[Path, Path]:
    destination = local_vcpkg_path(root, system).parent
    if not confirm(f"vcpkg is missing. Clone and bootstrap pinned vcpkg in '{destination}'?", assume_yes):
        raise SetupError("vcpkg provisioning was declined.")
    if offline:
        raise SetupError("vcpkg is missing and offline mode prevents cloning its source.")

    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_dir():
        result = run(["git", "-C", str(destination), "rev-parse", "HEAD"], capture=True)
        if result.returncode != 0 or result.stdout.strip() != VCPKG_COMMIT:
            raise SetupError(f"The existing vcpkg source at '{destination}' is not the pinned commit. Remove it and rerun setup.")
    else:
        result = run(["git", "init", str(destination)])
        if result.returncode != 0:
            raise SetupError("Creating the local vcpkg source checkout failed.")
        result = run(["git", "-C", str(destination), "remote", "add", "origin", VCPKG_REPOSITORY])
        if result.returncode != 0:
            raise SetupError("Configuring the local vcpkg source checkout failed.")
        result = run(["git", "-C", str(destination), "fetch", "--depth", "1", "origin", VCPKG_COMMIT])
        if result.returncode != 0:
            raise SetupError("Fetching the pinned vcpkg source failed.")
        result = run(["git", "-C", str(destination), "checkout", "--detach", "FETCH_HEAD"])
        if result.returncode != 0:
            raise SetupError("Checking out the pinned vcpkg source failed.")

    bootstrap = destination / ("bootstrap-vcpkg.bat" if system == "windows" else "bootstrap-vcpkg.sh")
    command = [str(bootstrap), "-disableMetrics"] if system == "windows" else ["sh", str(bootstrap), "-disableMetrics"]
    result = run(command, cwd=destination)
    executable = local_vcpkg_path(root, system)
    if result.returncode != 0 or not executable.is_file():
        raise SetupError("Bootstrapping vcpkg failed.")
    return executable, destination


def install_vcpkg_dependencies(
    executable: Path,
    vcpkg_root: Path,
    root: Path,
    system: str,
    offline: bool,
    vcvars: Path | None,
) -> None:
    installed = root / "build" / "vcpkg_installed" / system
    status = installed / "vcpkg" / "status"
    if offline:
        if not status.is_file():
            raise SetupError("Native dependencies are missing and offline mode prevents installing them.")
        print("Offline mode: using the existing native dependencies without reconciling the manifest.")
        return

    triplet = "x64-windows" if system == "windows" else "x64-linux"
    environment = clean_environment(os.environ)
    environment["VCPKG_ROOT"] = str(vcpkg_root)
    environment["VCPKG_DISABLE_METRICS"] = "1"
    if vcvars:
        environment["VCPKG_VISUAL_STUDIO_PATH"] = str(vcvars.parents[3])
    command = [
        str(executable),
        "install",
        f"--x-manifest-root={root}",
        f"--x-install-root={installed}",
        f"--triplet={triplet}",
    ]
    result = run(command, cwd=root, environment=environment)
    if result.returncode != 0:
        raise SetupError("Installing the vcpkg manifest failed.")


def find_ctest(system: str) -> Path:
    candidates: list[Path] = []
    discovered = shutil.which("ctest")
    if discovered:
        candidates.append(Path(discovered))
    if system == "windows":
        program_files = os.environ.get("ProgramFiles")
        if program_files:
            candidates.append(Path(program_files) / "CMake" / "bin" / "ctest.exe")

    for candidate in candidates:
        version = executable_version(candidate, ["--version"])
        if version and version_at_least(version, CTEST_MINIMUM_VERSION):
            return candidate
    raise SetupError("Standalone CTest 3.21 or newer was not found. Install CTest and rerun setup.")


def find_ninja(root: Path) -> Path | None:
    system = host_system()
    if system != "linux":
        return None

    candidates: list[Path] = []
    discovered = shutil.which("ninja")
    if discovered:
        candidates.append(Path(discovered))
    candidates.append(local_ninja_path(root, system))

    for candidate in candidates:
        version = executable_version(candidate, ["--version"])
        if version and version_at_least(version, NINJA_MINIMUM_VERSION):
            return candidate
    return None


def build_info_document(
    *,
    action: str,
    compiler: str,
    premake: Path,
    build_tool: Path,
    ctest: Path,
    vcpkg_root: Path,
    vcvars: Path | None,
) -> dict[str, str | None]:
    return {
        "action": action,
        "compiler": compiler,
        "premake": str(premake.resolve()),
        "buildTool": str(build_tool.resolve()),
        "ctest": str(ctest.resolve()),
        "vcpkgRoot": str(vcpkg_root.resolve()),
        "vcvars": str(vcvars.resolve()) if vcvars else None,
    }


def write_build_info(root: Path, **arguments: object) -> None:
    output = root / ".eppo" / "build.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(build_info_document(**arguments), indent=2) + "\n", encoding="utf-8")


def choose_action(requested: str | None, system: str) -> str:
    if requested:
        if requested not in SUPPORTED_ACTIONS:
            raise SetupError(f"Unsupported action '{requested}'.")
        if system == "windows" and requested == "ninja":
            raise SetupError("Windows supports the VS2022 and VS2026 actions. Ninja is supported on Linux.")
        if system == "linux" and requested != "ninja":
            raise SetupError("Linux currently supports the Ninja action.")
        return requested

    selected = default_action(system)
    if selected:
        return selected
    if system != "windows":
        raise SetupError(f"Unsupported host platform '{system}'.")

    print("Select generated build files:")
    for index, action in enumerate(WINDOWS_ACTIONS, start=1):
        print(f"  {index}. {action}")
    response = input("Selection: ").strip()
    try:
        return WINDOWS_ACTIONS[int(response) - 1]
    except (ValueError, IndexError):
        raise SetupError("Invalid generator selection.") from None


def validate_environment(system: str, action: str, compiler: str) -> tuple[Path | None, Path]:
    missing = [name for name in ("VULKAN_SDK", "DOTNET_ROOT") if not os.environ.get(name)]
    if missing:
        raise SetupError(f"Missing required environment variables: {', '.join(missing)}")

    dotnet = Path(os.environ["DOTNET_ROOT"]) / ("dotnet.exe" if system == "windows" else "dotnet")
    version = executable_version(dotnet, ["--version"])
    if not version or not version.startswith("10."):
        raise SetupError(f"DOTNET_ROOT must reference a .NET 10 SDK; found '{version or 'nothing'}'.")

    vulkan = Path(os.environ["VULKAN_SDK"])
    include = vulkan / ("Include" if system == "windows" else "include")
    library = vulkan / ("Lib" if system == "windows" else "lib")
    if not include.is_dir() or not library.is_dir():
        raise SetupError("VULKAN_SDK does not contain the expected include and library directories.")
    dxc = vulkan / ("Bin/dxc.exe" if system == "windows" else "bin/dxc")
    dxcompiler = vulkan / ("Bin/dxcompiler.dll" if system == "windows" else "lib/libdxcompiler.so")
    if not dxc.is_file() or not dxcompiler.is_file():
        raise SetupError("VULKAN_SDK must provide dxc and the dxcompiler runtime.")

    if system == "windows":
        vcvars, msbuild = find_windows_toolchain(action)
        if compiler == "clang":
            visual_studio = vcvars.parents[3]
            clang_candidates = [
                visual_studio / "VC/Tools/Llvm/x64/bin/clang-cl.exe",
                visual_studio / "VC/Tools/Llvm/bin/clang-cl.exe",
            ]
            discovered_clang = shutil.which("clang-cl")
            if discovered_clang:
                clang_candidates.append(Path(discovered_clang))
            if not any(candidate.is_file() for candidate in clang_candidates):
                raise SetupError("The selected Visual Studio installation does not provide the Clang compiler toolset.")
        return vcvars, msbuild
    elif not shutil.which("clang") or not shutil.which("clang++"):
        raise SetupError("Linux builds require clang and clang++ on PATH.")
    return None, Path("ninja")


def generate(premake: Path, root: Path, action: str, compiler: str | None, vcpkg_root: Path) -> None:
    command = [str(premake), action]
    if compiler:
        command.append(f"--cc={compiler}")
    environment = clean_environment(os.environ)
    environment["VCPKG_ROOT"] = str(vcpkg_root)
    result = run(command, cwd=root, environment=environment)
    if result.returncode != 0:
        raise SetupError(f"Premake generation for {action} failed.")


def write_compile_commands(root: Path, system: str) -> None:
    # A clangd/LSP compilation database. Paths are machine-specific, so it is
    # generated (never committed) and refreshed on every setup, mirroring the
    # include dirs and defines in Scripts/Premake/Dependencies.lua. clang-cl on
    # Windows auto-resolves the MSVC STL + Windows SDK; the build generators do
    # not emit a compile database, so clangd would otherwise have no include paths.
    triplet = "x64-windows" if system == "windows" else "x64-linux"
    vcpkg_include = (root / "build" / "vcpkg_installed" / system / triplet / "include").as_posix()
    common = [vcpkg_include, f"{vcpkg_include}/tracy"]
    vulkan = os.environ.get("VULKAN_SDK")
    if vulkan:
        common.append((Path(vulkan) / ("Include" if system == "windows" else "include")).as_posix())

    engine = (root / "EppoEngine" / "Source").as_posix()
    vendor = (root / "EppoEngine" / "Vendor").as_posix()
    projects = {
        "EppoEngine": ([root / "EppoEngine" / "Source", root / "EppoEngine" / "Vendor"], [engine, vendor]),
        "EppoEditor": ([root / "EppoEditor" / "Source"], [(root / "EppoEditor" / "Source").as_posix(), engine]),
        "EppoRuntime": ([root / "EppoRuntime" / "Source"], [(root / "EppoRuntime" / "Source").as_posix(), engine]),
        "EppoEngineTesting": ([root / "EppoEngineTesting" / "Source"], [(root / "EppoEngineTesting" / "Source").as_posix(), engine]),
    }

    defines = ["GLM_FORCE_DEPTH_ZERO_TO_ONE", "EP_DEBUG", "TRACY_ENABLE"]
    defines.append("EP_PLATFORM_WINDOWS" if system == "windows" else "EP_PLATFORM_LINUX")
    if system == "linux":
        defines.append("__EMULATE_UUID")

    if system == "windows":
        base, define_flag, compile_flag = ["clang-cl", "/std:c++20", "/EHsc", "/utf-8"], "/D", "/c"
    else:
        base, define_flag, compile_flag = ["clang++", "-std=c++20", "-fexceptions"], "-D", "-c"

    entries: list[dict[str, object]] = []
    for source_roots, include_roots in projects.values():
        prefix = list(base)
        prefix += [f"-I{directory}" for directory in include_roots + common]
        prefix += [f"{define_flag}{macro}" for macro in defines]
        for source_root in source_roots:
            for source in sorted(source_root.rglob("*.cpp")):
                entries.append(
                    {"directory": root.as_posix(), "file": source.as_posix(), "arguments": prefix + [compile_flag, source.as_posix()]}
                )

    (root / "compile_commands.json").write_text(json.dumps(entries, indent=1) + "\n", encoding="utf-8")
    print(f"Wrote compile_commands.json ({len(entries)} translation units)")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Provision Eppo build tools and generate native build files.")
    parser.add_argument("--action", choices=SUPPORTED_ACTIONS)
    parser.add_argument("--compiler", choices=("msc", "clang"))
    parser.add_argument("--yes", action="store_true", help="Approve downloading missing Premake or vcpkg without prompting.")
    parser.add_argument("--offline", action="store_true", help="Do not access the network.")
    parser.add_argument("--no-generate", action="store_true", help="Provision and validate tools without running Premake.")
    parser.add_argument("--skip-dependencies", action="store_true", help="Do not run vcpkg install.")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    root = Path(__file__).resolve().parent.parent
    system = host_system()
    try:
        action = choose_action(arguments.action, system)
        compiler = arguments.compiler or ("msc" if system == "windows" else "clang")
        vcvars, default_build_tool = validate_environment(system, action, compiler)
        premake = find_premake(root, system)
        if not premake:
            premake = build_premake(root, system, action, arguments.yes, arguments.offline)
        print(f"Using Premake {PREMAKE_VERSION}: {premake}")
        vcpkg = find_vcpkg(root, system)
        if not vcpkg:
            vcpkg = provision_vcpkg(root, system, arguments.yes, arguments.offline)
        vcpkg_executable, vcpkg_root = vcpkg
        print(f"Using vcpkg: {vcpkg_executable}")
        if not arguments.skip_dependencies:
            install_vcpkg_dependencies(vcpkg_executable, vcpkg_root, root, system, arguments.offline, vcvars)
        ctest = find_ctest(system)
        build_tool = default_build_tool
        if action == "ninja":
            build_tool = find_ninja(root)
            if not build_tool:
                raise SetupError(f"Ninja {NINJA_MINIMUM_VERSION[0]}.{NINJA_MINIMUM_VERSION[1]} or newer was not found.")
        if not arguments.no_generate:
            generate(premake, root, action, compiler, vcpkg_root)
            write_compile_commands(root, system)
        write_build_info(
            root,
            action=action,
            compiler=compiler,
            premake=premake,
            build_tool=build_tool,
            ctest=ctest,
            vcpkg_root=vcpkg_root,
            vcvars=vcvars,
        )
        print(f"Generated action: {action} ({compiler})")
        return 0
    except SetupError as error:
        print(f"Setup failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
