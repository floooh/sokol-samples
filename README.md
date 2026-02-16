# sokol-samples

[![Build Status](https://github.com/floooh/sokol-samples/actions/workflows/build.yml/badge.svg)](https://github.com/floooh/sokol-samples/actions/workflows/build.yml)

Sample code for https://github.com/floooh/sokol

WASM live demos:

- for WebGL2: https://floooh.github.io/sokol-html5/index.html
- for WebGPU: https://floooh.github.io/sokol-webgpu/index.html

## How to Build on Windows, Linux and macOS

First check that the following tools are in the path. Exact versions shouldn't matter
(on Windows you can use [scoop.sh](https://scoop.sh/) to easily install the required tools from the
command line):

```sh
> git --version
2.x.x (any version should do)
> python3 --version
Python 3.x
> cmake --version
cmake version 3.21.x or later
# on OSX (on Windows you just need a recent VS)
> xcodebuild -version
Xcode xx.x (newer is better, older version are no longer tested)
# ninja is recommended for cmdline builds, faster and also cleaner terminal output
> ninja --version
1.x.y
```

> [!NOTE]
> On Linux you'll also need to install packages for OpenGL, X11, ALSA and Vulkan development, e.g.
> mesa-common-dev, libx11-dev, libasound2-dev, libvulkan-dev, vulkan-validationlayers, vulkan-tools

> [!NOTE]
> On Windows with Vulkan development you need to install the Vulkan SDK and before building,
> make sure that the environement variable `VULKAN_SDK` is valid

Create a scratch/workspace dir and clone the project:
```sh
> mkdir ~/scratch
> cd ~/scratch
> git clone https://github.com/floooh/sokol-samples
> cd sokol-samples
```

Select a build config for your platform and 3D backend combination:
```sh
# macOS with Metal:
> ./fips set config sapp-metal-osx-ninja-debug
# macOS with OpenGL:
> ./fips set config sapp-osx-ninja-debug
# Windows with D3D11:
> ./fips set config sapp-d3d11-win64-vstudio-debug
# Windows with OpenGL:
> ./fips set config sapp-win64-vstudio-debug
# Windows with Vulkan:
> ./fips set config sapp-vk-win64-vstudio-debug
# Linux with OpenGL
> ./fips set config sapp-linux-ninja-debug
# Linux with Vulkan:
> ./fips set config sapp-vk-linux-ninja-debug
```

Build the project (this will also fetch additional dependencies):
```sh
> ./fips build
```

List and run the build targets:
```sh
> ./fips list targets
...
> ./fips run triangle-sapp
```

...to open the project in Visual Studio or Xcode:
```sh
> ./fips gen
> ./fips open
```

...you can also open the project in VSCode (with the MS C/C++ extension),
for instance on Linux:
```sh
> ./fips set config linux-vscode-debug
> ./fips gen
> ./fips open
```

For additional platforms (like iOS/Android), continue reading the README past the What's New section.

For more information on the fips build system see here: https://floooh.github.io/fips/

## Building the platform-agnostic sokol_app.h + sokol_gfx.h samples for additional platforms:

Building the sokol_app.h samples is currently supported for MacOS, Windows,
Linux, iOS, HTML5 and Android.

Use any of the following custom build configs starting with ```sapp-```
which matches your platform and build system:

```bash
> ./fips list configs | grep sapp-
  sapp-android-ninja-debug
  ...
  sapp-d3d11-win64-vs2017-debug
  sapp-d3d11-win64-vs2017-release
  sapp-d3d11-win64-vscode-debug
  sapp-d3d11-win64-vstudio-debug
  sapp-d3d11-win64-vstudio-release
  sapp-ios-xcode-debug
  ...
  sapp-win64-vstudio-debug
  sapp-win64-vstudio-release
  ...
  sapp-webgl2-wasm-ninja-debug
  sapp-webgl2-wasm-ninja-release
  sapp-webgl2-wasm-vscode-debug
  sapp-webgl2-wasm-vscode-release
  ...
  sapp-wgpu-wasm-ninja-debug
  sapp-wgpu-wasm-ninja-release
  sapp-wgpu-wasm-vscode-debug
  sapp-wgpu-wasm-vscode-release

> ./fips set config sapp-...
> ./fips build
> ./fips list targets
> ./fips run cube-sapp
```

Note the following caveats:
- for HTML5, first install the emscripten SDK as described above in the
  native HTML5 sample section
- for iOS, set the developer team id, as described above in the iOS section

### Building with WebGPU backend on native desktop platforms

On Windows, macOS and Linux you can also build the samples for the
`SOKOL_WGPU` backend by linking against Google's Dawn (other WebGPU
implementations might work but are not tested so far).

First fetch and build Dawn into a DLL (this can take anywhere between
10 minutes and an hour depending on your setup):

```bash
> ./fips dawn install
```

...then select one of the following build configs:

```
> ./fips set config sapp-wgpu-linux-ninja-release
> ./fips set config sapp-wgpu-osx-ninja-release
> ./fips set config sapp-wgpu-win64-vstudio-release
```

...and build and run the samples:

```bash
> ./fips build
> ./fips run clear-sapp-ui
```

### Building the platform-specific samples

There are two types of samples, platform-specific samples in the
folders ```d3d11```, ```glfw```, ```html5``` and ```metal```, and
platform-agnostic samples using the ```sokol_app.h``` application-wrapper
header in the folder ```sapp```.

### To build the GLFW samples on Linux, MacOS and Windows:

```
> mkdir ~/scratch
> cd ~/scratch
> git clone https://github.com/floooh/sokol-samples
> cd sokol-samples
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-glfw
...
```

On Linux you'll need to install a couple of development packages for
GLFW: http://www.glfw.org/docs/latest/compile.html#compile_deps_x11

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

### To build for WebGL2+WASM on Emscripten:

```
> cd ~/scratch/sokol-samples
> ./fips setup emscripten
[...this will take a while]
> ./fips set config webgl2-wasm-ninja-release
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-emsc
...
```

### To build for Android:

Plug an Android device into your computer, and then:

```
> cd ~/scratch/sokol-samples
> ./fips setup android
[...this will install a local Android SDK/NDK under ../fips-sdks/android]
> ./fips set config sapp-android-ninja-debug
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-sapp
...
```
The last command should install and run the ```triangle-sapp``` sample on the
connected Android device.

To debug Android applications I recommend using Android Studio with
"Profile or debug APK". You can find the compiled APK files under
```../fips-deploy/[project]/[config]```.

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
> cc cube-sapp.c ../libs/sokol/sokol.m -o cube-sapp -DSOKOL_METAL -fobjc-arc -I../../sokol -I ../libs -framework Metal -framework Cocoa -framework MetalKit -framework QuartzCore -framework AudioToolbox
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
- Handmade-Math: https://github.com/StrangeZak/Handmade-Math
- Dear Imgui: https://github.com/ocornut/imgui
- cimgui: https://github.com/cimgui/cimgui
- utest.h: https://github.com/sheredom/utest.h
- Ozz Animation: https://github.com/guillaumeblanc/ozz-animation

Enjoy!

## What's New:

(FIXME: this stuff really needs to go into a separate CHANGELOG.md)

- **21-Oct-2023**: Updated WebGPU support, and new samples .

(lots of stuff missing here)

- **23-Sep-2020**: samples can now be built for UWP using the ```sapp-uwp-vstudio-debug``` and ```sapp-uwp-vstudio-release``` build configs
  NOTE that fips support for UWP built apps is incomplete (e.g. ```fips run``` doesn't work, and UWP app bundle creation is also not supported)

- **27-May-2020**: four new test and demonstration samples for the new sokol_debugtext.h header

- **30-Apr-2020**: New sokol\_gfx.h WebGPU backend samples, and updated all other samples for the breaking changes in sokol\_gfx.h initialization, see the [Updates](https://github.com/floooh/sokol#updates) section in the sokol\_gfx.h README for details!

- **24-Feb-2020**: I have added a [section to the readme](https://github.com/floooh/sokol-samples#how-to-build-without-a-build-system) with examples of how to build (most of) the examples without a build system by invoking the C compiler directly on the command line.
This might be useful for integration of the sokol headers into your own projects using your
own preferred build system.

- **22-Jan-2020**: New sample to demonstrate how to render from inside a Dear ImGui user draw callback: [imgui-usercallback-sapp](https://floooh.github.io/sokol-html5/imgui-usercallback-sapp.html)

- **26-Aug-2019**: New sample: [fontstash-sapp](https://floooh.github.io/sokol-html5/fontstash-sapp.html)

- **06-Jul-2019**: Two new samples for the new [sokol_fetch.h header](https://github.com/floooh/sokol/blob/master/sokol_fetch.h):
    - [loadpng-sapp](https://floooh.github.io/sokol-html5/loadpng-sapp.html): load an image file into a sokol-gfx texture
    - [plmpeg-sapp](https://floooh.github.io/sokol-html5/plmpeg-sapp.html): MPEG1 streaming via [pl_mpeg](https://github.com/phoboslab/pl_mpeg)

- **04-Jun-2019**: New sample on how to compile and use the sokol headers as
DLL (currently only on Windows). This demonstrates the new SOKOL_DLL
configuration define which annotates public function declarations with
__declspec(dllexport) or __declspec(dllimport). [See
here](https://github.com/floooh/sokol-samples/tree/master/libs/sokol) for the
DLL, [and here](https://github.com/floooh/sokol-samples/blob/master/sapp/noentry-dll-sapp.c)
for the example code using the DLL.

- **15-May-2019**: the sokol-app samples in the ```sapp``` directory have
been "ported" to the new shader-cross-compiler solution ([see here for
details](https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md)).
Shaders are written as 'annotated GLSL', and cross-compiled to various
GLSL dialects, HLSL and MSL through a custom-build job which invokes the
```sokol-shdc``` command line tool.

- **01-Apr-2019**: sample code for the new sokol_gl.h header:
    - [sapp/sgl-sapp.c](https://github.com/floooh/sokol-samples/blob/master/sapp/sgl-sapp.c): triangles, quads, texturing and the matrix stack
    - [sapp/sgl-lines-sapp.c](https://github.com/floooh/sokol-samples/blob/master/sapp/sgl-lines-sapp.c): lines and line strips
    - [sapp/sgl-microui-sapp.c](https://github.com/floooh/sokol-samples/blob/master/sapp/sgl-microui-sapp.c): example [microui](https://github.com/rxi/microui) integration
    - [glfw/sgl-test-glfw.c](https://github.com/floooh/sokol-samples/blob/master/glfw/sgl-test-glfw.c): a pure GLFW/OpenGL 1.2 program to check whether sokol_gl.h behaves the same as OpenGL 1.2

- **05-Mar-2019**: the sokol-app samples (in the sapp directory) now come with optional
debugging UIs implemented via the new Dear ImGui based debug-inspection headers, these
are compiled as separate executables, so the executable-versions without UI are still as
small as possible.

- **19-Feb-2019**: a new sokol_app.h sample has been added to demonstrate the
new SOKOL_NO_ENTRY feature (in which sokol_app.h doesn't hijack the main function):
```sapp/noentry-sapp.c```

- **26-Jan-2019**: The sokol_app.h samples now also work on Android. See below for build instructions.

- **12-Apr-2018**: New samples have been added to demonstrate the new optional vertex-buffer-
and index-buffer-offsets in the sg\_draw\_state struct. Also the location of fips build-system
files have changed, please update fips with a 'git pull' from the fips directory.

- **27-Mar-2018**: The Dear Imgui fips wrapper has recently been moved to a new repository at
https://github.com/fips-libs/fips-imgui and updated to the latest ImGui version which
required some code changes. If you already had checked out sokol-samples, perform the following
steps to udpate:
    1. delete the fips-imgui directory
    2. in the sokol-samples directory, run **./fips fetch**
