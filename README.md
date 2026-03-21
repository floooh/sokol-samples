# sokol-samples

[![Build Status](https://github.com/floooh/sokol-samples/actions/workflows/build.yml/badge.svg)](https://github.com/floooh/sokol-samples/actions/workflows/build.yml)

Sample code for https://github.com/floooh/sokol

WASM live demos:

- for WebGL2: https://floooh.github.io/sokol-html5/index.html
- for WebGPU: https://floooh.github.io/sokol-webgpu/index.html

## Prerequisites

Make sure the following tools are installed:

- deno (https://docs.deno.com/runtime/getting_started/installation/)
- cmake
- your system's default C/C++ toolchain (MSVC, GCC or Clang)
- optional but recommended: ninja

> [!NOTE]
> On Linux you'll also need to install packages for OpenGL, X11, ALSA and Vulkan development, e.g.
> libgl1-mesa-dev, libegl1-mesa-dev, mesa-common-dev, xorg-dev, libasound-dev, libvulkan-dev, vulkan-validationlayers,
> vulkan-tools

> [!NOTE]
> On **Windows** with Vulkan development you need to install the Vulkan SDK and before building,
> make sure that the environement variable `VULKAN_SDK` is valid

> [!NOTE]
> On **Windows** run the build in a 'Visual Studio Command Prompt' terminal window.

## Build and running samples

To build the samples in the host platform's default release-mode configuration:

```sh
git clone https://github.com/floooh/sokol-samples
./fibs build
```

...then list all executable samples:

```sh
./fibs list targets --exe
```

...and run any of the samples, e.g.:
```sh
./fisb run triangle-sapp-ui
```

Alternatively configure for a specific build configuration:

```sh
./fibs config [build-config-name]
```

To see a list of all build configs:

```sh
./fibs list configs
```

To search for specific build configs, use `grep` or `rg` (ripgrep):

```sh
./fibs list configs | grep d3d11
./fibs list configs | rg macos
```

To open a project in an IDE, first configure with a build config
which has `-vstudio-`, `-vscode-` or `-xcode-` in the name, and
then run `./fibs open`, e.g.:

```sh
./fibs config sapp-metal-macos-vscode-debug
./fibs open
```

...for additional documentation on the fibs meta-build-system see:

- https://github.com/floooh/fibs

## Cross-compile to Emscripten, iOS or Android

### Emscripten

First install the Emscripten SDK (this will take a while):

```sh
./fibs emsdk install
```

Then pick one of the build configs with `-emsc-` in the name, build and run:

```sh
./fibs config sapp-gles-emsc-ninja-debug
./fibs build
./fibs run triangle-sapp
```

...this should open the system default web browser, running the selected sample.

### iOS

Pick an iOS build config and open in Xcode:

```sh
./fibs config sapp-metal-ios-xcode-debug
./fibs open
```

...then build and run in the iOS simulator or iOS device.

To build and run on a real device you may need to provide an `iOS Team Id`, do
this via:

```sh
./fibs set iosteamid [your-team-id]
./fibs config sapp-metal-ios-xcode-debug
```

### Android

Install a JDK version 17, and check via `./fibs diag tools` whether
fibs can find `java`, `javac` can be found and `unzip`:

```sh
./fibs diag tools
...
java:   found
unzip:  found
cwebp:  found
```

...next install the Android SDK/NDK via:

```sh
./fibs android install
```

...then pick one of the Android build configs and build:

```sh
./fibs config sapp-gles-android-ninja-debug
./fibs build
```

...finally run any of the samples on an attached Android device:

```sh
./fibs run triangle-sapp
```

## How to build without a build system

Many samples are simple enough to be built directly on the command line
without a build system, and the following examples of that might
be helpful for integrating the sokol headers and/or the sokol sample code
into your own project with the build system of your choice.

The expected directory structure for building the sokol-samples manually
is as follows (e.g. those directories must be cloned side-by-side):

```
sokol-samples
sokol
sokol-tools-bin
```

For instance:

```
> mkdir scratch
> cd scratch
> git clone https://github.com/floooh/sokol-samples
> git clone https://github.com/floooh/sokol
> git clone https://github.com/floooh/sokol-tools-bin
```

Of course, in your own project you can put the sokol headers wherever
you want (I recommend copying them somewhere into your source directory),
and you don't have to use the prebuilt sokol-shdc shader compiler either.

> [!NOTE]
> For Release builds, you might want to add the compiler's respective
optimization flags, and provide an ```NDEBUG``` define so that assert() checks
and the sokol-gfx validation layer are removed (**BUT** please don't do
this for the Debug/Dev builds because asserts() and the validation
layer give important feedback if something goes wrong).

### Building manually on macOS with clang

To build one of the Metal samples:

```sh
> cd sokol-samples/metal
> cc cube-metal.c osxentry.m sokol_gfx.m -o cube-metal -fobjc-arc -I../../sokol -framework Metal -framework Cocoa -framework QuartzCore
```

To build one of the GLFW samples (requires a system-wide glfw install, e.g. ```brew install glfw```):

```sh
> cd sokol-samples/glfw
> cc cube-glfw.c flextgl/flextGL.c -o cube-glfw -I../../sokol -lglfw -framework OpenGL -framework Cocoa
```

To build one of the sokol-app samples for Metal:

```sh
> cd sokol-samples/sapp
> ../../sokol-tools-bin/bin/osx/sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l metal_macos
> cc cube-sapp.c ../libs/sokol/sokol.m -o cube-sapp -DSOKOL_METAL -fobjc-arc -I../../sokol -I ../libs -framework Metal -framework Cocoa -framework QuartzCore -framework AudioToolbox
```

To build one of the sokol-app samples for GL on macOS:

```sh
> cd sokol-samples/sapp
> ../../sokol-tools-bin/bin/osx/sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l glsl430
> cc cube-sapp.c ../libs/sokol/sokol.m -o cube-sapp -fobjc-arc -DSOKOL_GLCORE -I../../sokol -I../libs -framework OpenGL -framework Cocoa -framework AudioToolbox
```

### Building manually on Windows with MSVC

From the ```VSxxxx x64 Native Tools Command Prompt``` (for 64-bit builds)
or ```Developer Command Prompt for VSxxxx``` (for 32-bit builds):

To build one of the D3D11 samples:

```sh
> cd sokol-samples\d3d11
> cl cube-d3d11.c d3d11entry.c /I..\..\sokol
```

To build one of the sokol-app samples for D3D11:

```sh
> cd sokol-samples\sapp
> ..\..\sokol-tools-bin\bin\win32\sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l hlsl5
> cl cube-sapp.c ..\libs\sokol\sokol.c /DSOKOL_D3D11 /I..\..\sokol /I..\libs
```

To build one of the sokol-app samples for GL on Windows:

```sh
> cd sokol-samples\sapp
> ..\..\sokol-tools-bin\bin\win32\sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l glsl430
> cl cube-sapp.c ..\libs\sokol\sokol.c /DSOKOL_GLCORE /I..\..\sokol /I..\libs kernel32.lib user32.lib gdi32.lib
```

### Building manually on Windows with MSYS2/mingw gcc:

> [!NOTE]
> Compile with '-mwin32' (this defines _WIN32 for proper platform detection)

From the MSYS2 shell:

```sh
> cd sokol-samples/sapp
# compile shaders for HLSL and GLSL:
> ../../sokol-tools-bin/bin/win32/sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l hlsl5:glsl430
# build and run with D3D11 backend:
> gcc cube-sapp.c ../libs/sokol/sokol.c -o cube-sapp-d3d11 -mwin32 -O2 -DSOKOL_D3D11 -I../../sokol -I ../libs -lkernel32 -luser32 -lshell32 -ldxgi -ld3d11 -lole32 -lgdi32
> ./cube-sapp-d3d11
# build and run with GL backend:
> gcc cube-sapp.c ../libs/sokol/sokol.c -o cube-sapp-gl -mwin32 -O2 -DSOKOL_GLCORE -I../../sokol -I ../libs -lkernel32 -luser32 -lshell32 -lgdi32 -lole32
> ./cube-sapp-gl
```

### Building manually on Windows with Clang:

> [!NOTE]
> AFAIK Clang for Windows needs a working MSVC toolchain and Windows SDK installed, I haven't tested without.

Clang recognizes the ```#pragma comment(lib,...)``` statements in the Sokol headers, so you don't
need to specify the link libraries manually.

```sh
> cd sokol-samples/sapp
# compile shaders for HLSL and GLSL:
> ..\..\sokol-tools-bin\bin\win32\sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l hlsl5:glsl430
# build and run with D3D11 backend:
> clang cube-sapp.c ../libs/sokol/sokol.c -o cube-sapp-d3d11.exe -O2 -DSOKOL_D3D11 -I ../../sokol -I ../libs
> cube-sapp-d3d11
# build and run with GL backend:
> clang cube-sapp.c ../libs/sokol/sokol.c -o cube-sapp-gl.exe -O2 -DSOKOL_GLCORE -I ../../sokol -I ../libs
> cube-sapp-gl
```

### Building manually on Linux with gcc

On Linux you need the "usual" development-packages for OpenGL development, and
for the GLFW samples, also the GLFW development package (on Ubuntu it's called
libglfw3-dev).

To build one of the GLFW samples on Linux:

```sh
> cd sokol-samples/glfw
> cc cube-glfw.c glfw_glue.c flextgl/flextGL.c -o cube-glfw -I../../sokol -lGL -ldl -lm -lglfw3
```

To build one of the sokol-app samples on Linux:

```sh
> cd sokol-samples/sapp
> ../../sokol-tools-bin/bin/linux/sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l glsl430
> cc cube-sapp.c ../libs/sokol/sokol.c -o cube-sapp -DSOKOL_GLCORE -pthread -I../../sokol -I../libs -lGL -ldl -lm -lX11 -lasound -lXi -lXcursor
```

### Building for WASM / WebGL2

Make sure `emcc` and `emrun` from the Emscripten SDK are in the path.

```sh
> cd sokol-samples/html5
> emcc cube-emsc.c -o cube-emsc.html -I../../sokol -sUSE_WEBGL2 --shell-file=../webpage/shell.html
> emrun cube-emsc.html
```

...and for the sokol-app samples:

```sh
> sokol-samples/sapp
> ../../sokol-tools-bin/bin/[platform]/sokol-shdc -i cube-sapp.glsl -o cube-sapp.glsl.h -l glsl300es
> emcc cube-sapp.c ../libs/sokol/sokol.c -o cube-sapp.html -DSOKOL_GLES3 -I../../sokol -I../libs -sUSE_WEBGL2 --shell-file=../webpage/shell.html
> emrun cube-sapp.html
```

## Many Thanks to:

- GLFW: https://github.com/glfw/glfw
- flextGL: https://github.com/ginkgo/flextGL
- vecmath: https://github.com/mattiasgustavsson/libs/blob/main/docs/vecmath.md
- Dear Imgui: https://github.com/ocornut/imgui
- cimgui: https://github.com/cimgui/cimgui
- utest.h: https://github.com/sheredom/utest.h
- Ozz Animation: https://github.com/guillaumeblanc/ozz-animation

Enjoy!
