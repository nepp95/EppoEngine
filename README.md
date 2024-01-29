# EppoEngine
[![Master Build](https://github.com/nepp95/EppoEngine/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/nepp95/EppoEngine/actions/workflows/build.yml)
[![Development Build](https://github.com/nepp95/EppoEngine/actions/workflows/build.yml/badge.svg?branch=develop)](https://github.com/nepp95/EppoEngine/actions/workflows/build.yml)

## Introduction

EppoEngine is a personal project wherein I further my skills in C++ and graphical programming. It's main usage is as a game engine.

Where I have used C++ in conjuction with OpenGL and C# for scripting - which is common in game engines - in a previous project, I am now pointing my eyes at Vulkan, which in some way could be called the sequel of OpenGL. Vulkan, however, is much more explicit and gives you a lot of power. With great power comes great responsibility, so I will be learning a lot of the best practices and try to learn as much as possible.

## Planned Features

I will be focussing on creating a MVP as soon as possible, which will include the following:
- Basic Vulkan rendering setup
    - Render primitives (triangles, quads, circles, lines)
    - Render images from file
    - Render a scene within ImGui
- Basic light sources and shadows
- User input
    - Controlling the camera
    - Tweaking parameters in runtime

After this, I will be shifting my focus to more advanced topics and also some features that are mandatory in a production game (engine):
- Audio
- Networking
- Postprocessing FX

As a final point; I also want to dip my toes into raytracing and multithreading. Whether they will be part of the MVP or come after is yet to be seen, but I will at some point experiment with them.

## Installation

### Windows
*Note: Currently, Windows is the only supported platform.*

1. `git clone https://github.com/nepp95/EppoEngine.git` to a folder of your choosing.
2. Run `Setup.bat` from the `Scripts` folder.
This will require you to have Visual Studio 2022 installed. If you have a different version of Visual Studio installed or want to use CMake, please edit `GenerateProjects-Win.bat` and change `vs2022` to one of the options on [this page](https://premake.github.io/docs/Using-Premake).
3. Based on what you had installed on your computer before running `Setup.bat`, you might have to run it again. Please make sure you have run this program twice to verify everything is in order. If the program tells you "Done", you know it's okay!
4. You can now open the solution (Visual Studio), build and run! Of course, if you used a different parameter in step 2, you can use that build system to build out of the box.

### Linux
1. `git clone https://github.com/nepp95/EppoEngine.git` to a folder of your choosing.
2. Make sure you have the following dependencies installed:
```
sudo apt install libassimp-dev
```