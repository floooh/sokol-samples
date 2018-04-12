# sokol-samples

Sample code for https://github.com/floooh/sokol

asm.js/wasm live demos: https://floooh.github.io/sokol-html5/index.html

*Work In Progress*

### Build Status:

|Platform|Build Status|
|--------|------|
|Windows|[![Build status](https://ci.appveyor.com/api/projects/status/3jxh6gi272i5jd84/branch/master?svg=true)](https://ci.appveyor.com/project/floooh/sokol-samples/branch/master)|
|OSX|[![Build Status](https://travis-ci.org/floooh/sokol-samples.svg?branch=master)](https://travis-ci.org/floooh/sokol-samples)|

## Public Service Announcements

- **12-Apr-2018**: New samples have been added to demonstrate the new optional vertex-buffer- 
and index-buffer-offsets in the sg\_draw\_state struct. Also the location of fips build-system
files have changed, please update fips with a 'git pull' from the fips directory.

- **27-Mar-2018**: The Dear Imgui fips wrapper has recently been moved to a new repository at
https://github.com/fips-libs/fips-imgui and updated to the latest ImGui version which 
required some code changes. If you already had checked out sokol-samples, perform the following
steps to udpate:
    1. delete the fips-imgui directory
    2. in the sokol-samples directory, run **./fips fetch**

## How to build

Make sure that the following tools are in the path. Exact versions shouldn't
matter:
```
> python --version
Python 2.7.10
> cmake --version
cmake version 3.8.2
# make is only needed for building through emscripten
> make --version
GNU Make 3.81
# on OSX (on Windows you just need a recent VS)
> xcodebuild -version
Xcode 9.0
```

### Clone, build and run the native samples:
```
> mkdir ~/scratch
> cd ~/scratch
> git clone git@github.com:floooh/sokol-samples
> cd sokol-samples
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-glfw
...
```

### To open the project in Xcode or Visual Studio:

```
> cd ~/scratch/sokol-samples
> ./fips open
```

### To build for emscripten:
```
> cd ~/scratch/sokol-samples
> ./fips setup emscripten
[...this will take a while]
> ./fips set config webgl2-emsc-make-release
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-emsc
...
```

### To build for Metal on OSX:
```
> cd ~/scratch/sokol-samples
> ./fips set config metal-osx-xcode-debug
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-metal
```

### To build for Metal on iOS:
```
> cd ~/scratch/sokol-samples
> ./fips set config metal-ios-xcode-debug
> ./fips set iosteam [YOUR-TEAM-ID]
> ./fips gen
> ./fips open
# Xcode should open now, where you can build and run the iOS code as usual
```
The \[YOUR-TEAM-ID\] must be replaced with your Apple Developer Team ID, this
is a 10-character string which you can look up on
https://developer.apple.com/account/#/membership. If you get build errors
about 32-bit targets, exit Xcode, run ```./fips clean```, ```./fips gen```
and ```./fips open``` again. This is a known but unsolved issue which I need
to investigate.

Another known issue: The arraytex-metal sample currently has a weird rendering artefact at least on my iPad Mini4 which looks like Z-fighting.

### To build for D3D11 on Windows:
```
> cd /scratch/sokol-samples
> fips set config d3d11-win64-vstudio-debug
> fips build
...
> fips list targets
...
> fips run triangle-d3d11
```

### To build for Linux:
```
> cd ~/scratch/sokol-samples
> ./fips set config linux-make-debug
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-glfw
...
```
You may need to install some dev-packages required for GLFW on Linux,
see here: http://www.glfw.org/docs/latest/compile.html#compile_deps_x11

Type ```./fips help``` for more build system options.

## Thanks to:

- GLFW: https://github.com/glfw/glfw
- flextGL: https://github.com/ginkgo/flextGL
- Handmade-Math: https://github.com/StrangeZak/Handmade-Math
- Dear Imgui: https://github.com/ocornut/imgui

Enjoy!
