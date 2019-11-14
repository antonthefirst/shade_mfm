# shade_mfm
A GPU port of Dave Ackley's Movable Feast Machine

https://movablefeastmachine.org/

https://github.com/DaveAckley/MFM

## Videos
1. UI Basics: https://www.youtube.com/watch?v=tGccSJQMoGk
2. Programming: https://www.youtube.com/watch?v=PMUYluE6QLo

## System Requirements
OpenGL 4.3 compatible graphics card and drivers.

## Compiling shade_mfm on Ubuntu 18.04 (should work on 16.04)
0. Install Vulkan SDK from here: https://vulkan.lunarg.com/sdk/home. Make sure you can call glslc (shader compiler) from command line. 
1. Download or clone 
2. Unzip 
3. Install necessary libs and build essentials:
sudo apt install build-essential libglfw3-dev
4. run make 
5. when compiled just run ./shademfm

## Compiling shade_mfm on Windows
0. Install Vulkan SDK from here: https://vulkan.lunarg.com/sdk/home. Make sure the PATH variable is updated and that VULKAN_SDK/bin is visible (the code expects to be able to call glslc.exe (shader compiler) from command line)
1. Download or clone this repo
2. Unzip
3. Open shade_mfm.sln, it should load with Visual Studio 2015 (Professional or Community)
4. Compile and run

## Compiling shade_mfm on Mac
Doesn't work yet, but thanks to MoltenVK it might someday!

## Acknowledgements and Thanks
Dave Ackley for lengthy discussions and encouragement.

Alan Zaffetti for early adopting GPUlam and spurring on improvements to it.

Mariusz Sasinski for getting this to compile on Linux.

## Credits

UI powered by Dear ImGui: https://github.com/ocornut/imgui
Image reading/writing by stb: https://github.com/nothings/stb/