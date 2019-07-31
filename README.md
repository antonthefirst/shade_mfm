# shade_mfm
A GPU port of Dave Ackley's Movable Feast Machine

https://movablefeastmachine.org/

https://github.com/DaveAckley/MFM

## System Requirements
OpenGL 4.3 compatible graphics card and drivers.

## Compiling shade_mfm on Ubuntu 18.04 (should work on 16.04)
1. Download or clone 
2. Unzip 
3. Install necessary libs and build essentials:
sudo apt install build-essential libgl1-mesa-dev libglfw3-dev
4. run make 
5. when compiled just run ./shademfm

## Compiling shade_mfm on Windows
1. Download or clone
2. Unzip
3. Open shade_mfm.sln, it should load with Visual Studio 2015 (Professional or Community)
4. Compile and run

## Compiling shade_mfm on Mac
shade_mfm is written using GLSL compute shaders, which sadly aren't supported on Mac. Due to this, no Mac support is planned at this time :(

## Acknowledgements and Thanks
Dave Ackley for lengthy discussions and encouragement.

Alan Zaffetti for early adopting GPUlam and spurring on improvements to it.

Mariusz Sasinski for getting this to compile on Linux.