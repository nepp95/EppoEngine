# EppoEngine
[![Master Build](https://github.com/nepp95/EppoEngine/actions/workflows/ci-master.yml/badge.svg?branch=master)](https://github.com/nepp95/EppoEngine/actions/workflows/ci-master.yml)
[![Development Build](https://github.com/nepp95/EppoEngine/actions/workflows/ci-develop.yml/badge.svg?branch=develop)](https://github.com/nepp95/EppoEngine/actions/workflows/ci-develop.yml)

## Introduction

EppoEngine is a personal project wherein I further my skills in C++ and graphical programming. It's main usage is as a game engine.

Where I have used C++ in conjuction with OpenGL and C# for scripting - which is common in game engines - in a previous project, I am now pointing my eyes at Vulkan, which in some way could be called the sequel of OpenGL. Vulkan, however, is much more explicit and gives you a lot of power. With great power comes great responsibility, so I will be learning a lot of the best practices and try to learn as much as possible. Some of these include CPU to GPU synchronization and vice versa, multithreaded rendering, offscreen rendering, compute shaders and maybe even ray tracing acceleration structures.

*For the latest developments, checkout the `develop` branch since the `master` branch is rarely updated.*

## Planned Features

I will be focussing on creating a MVP as soon as possible, which will include the following:
- Basic rendering setup
    - [x] Render 3D meshes
    - [ ] Render billboards
- Basic light sources and shadows
    - [ ] Directional light
    - [x] Point light
- User input
    - [x] Controlling the camera (editor)
    - [ ] Controlling the camera (runtime)
    - [ ] Tweaking parameters in runtime
- Scripting language (C#)
    - [x] Implement basic scripting using Mono
    - [x] Modify existing entities (transformation for example)
    - [ ] Create new entities from C#
    - [ ] Control meshes from entities from C#

After this, I will be shifting my focus to more advanced topics and also some features that are mandatory in a production game (engine):
- Audio
- Networking
- Postprocessing FX
- Animation

## Installation
*Note: Currently, Windows is the only supported platform.*

### Windows

1. `git clone https://github.com/nepp95/EppoEngine.git --recursive` to a folder of your choosing. Make sure it is done recursively to also clone the submodules used in the project.
2. Run `Setup.bat` from the `Scripts` folder.
This will require you to have Visual Studio 2022 installed. If you have a different version of Visual Studio installed or want to use CMake, please edit `GenerateProjects-Win.bat` and change `vs2022` to one of the options on [this page](https://premake.github.io/docs/Using-Premake).
3. Based on what you had installed on your computer before running `Setup.bat`, you might have to run it again. Please make sure you have run this program twice to verify everything is in order. If the program tells you "Done", you know it's okay! In short, this downloads Premake if not found, downloads the VulkanSDK if not found and lastly generates the project files using said Premake. All of this is done using python, so you can inspect the scripts in the `Scripts` folder
4. You can now open the solution (Visual Studio), build and run! Of course, if you used a different parameter in step 2, you can use that build system to build out of the box.

### Linux
1. `git clone https://github.com/nepp95/EppoEngine.git` to a folder of your choosing.
2. Figure out how to build it using the premake5.lua files. They will be provided in the future.
