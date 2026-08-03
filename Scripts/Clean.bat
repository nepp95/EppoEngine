@ECHO OFF
SETLOCAL

SET "ROOT=%~dp0.."
IF NOT EXIST "%ROOT%\premake5.lua" GOTO InvalidRoot
IF NOT EXIST "%ROOT%\EppoEngine" GOTO InvalidRoot

PUSHD "%ROOT%" || EXIT /B 1

FOR %%D IN (
    "build"
    ".eppo"
    ".vcpkg-cache"
    "EppoEditor\Resources\Templates\NewProject\Scripts\bin"
    "EppoEditor\Resources\Templates\NewProject\Scripts\obj"
    "EppoEngineTesting\TestData\Scripts\bin"
    "EppoEngineTesting\TestData\Scripts\obj"
    "EppoScriptCore\bin"
    "EppoScriptCore\obj"
) DO IF EXIST "%%~D" RMDIR /S /Q "%%~D"

FOR /D %%P IN ("EppoEditor\Projects\*") DO (
    IF EXIST "%%~P\Scripts\bin" RMDIR /S /Q "%%~P\Scripts\bin"
    IF EXIST "%%~P\Scripts\obj" RMDIR /S /Q "%%~P\Scripts\obj"
)

FOR /D /R %%D IN (__pycache__) DO IF EXIST "%%~D" RMDIR /S /Q "%%~D"

FOR /R %%F IN (
    *.sln
    *.slnx
    *.vcxproj
    *.vcxproj.filters
    *.vcxproj.user
    *.ninja
    .ninja_deps
    .ninja_log
    compile_commands.json
    *.pyc
    *.pyo
) DO IF EXIST "%%~F" DEL /F /Q "%%~F"

POPD
ECHO Removed all generated Eppo build artifacts.
EXIT /B 0

:InvalidRoot
ECHO Refusing to clean unexpected repository root "%ROOT%".
EXIT /B 1
