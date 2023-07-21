@echo off
pushd %~dp0\..\
call Vendor\Premake\Bin\premake5.exe vs2022
popd
PAUSE
