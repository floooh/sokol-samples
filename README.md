# sokol-samples

Sample code for https://github.com/floooh/sokol

asm.js/wasm live demos: https://floooh.github.io/sokol-html5/index.html

*Work In Progress*

### Build Status:

|Platform|Build Status|
|--------|------|
|Windows|[![Build status](https://ci.appveyor.com/api/projects/status/3jxh6gi272i5jd84/branch/master?svg=true)](https://ci.appveyor.com/project/floooh/sokol-samples/branch/master)|
|Linux/OSX|[![Build Status](https://travis-ci.org/floooh/sokol-samples.svg?branch=master)](https://travis-ci.org/floooh/sokol-samples)|

## How to build

Currently only OSX and emscripten are tested. Linux and Windows may work too
but expect compile errors and warnings (for now).

Make sure that the following tools are in the path. Exact versions shouldn't
matter:
```
> python --version
Python 2.7.10
> cmake --version
cmake version 3.8.2
> make --version
GNU Make 3.81
> xcodebuild -version
Xcode 9.0
```

Clone, build and run the native samples:
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

To build for emscripten:
```
> cd ~/scratch/sokol-samples
> ./fips setup emscripten
[...this will take a while]
> ./fips set config emsc-make-release
> ./fips build
...
> ./fips list targets
...
> ./fips run triangle-emsc
...
```

## Thanks to:

- GLFW: https://github.com/glfw/glfw
- flextGL: https://github.com/ginkgo/flextGL
- Handmade-Math: https://github.com/StrangeZak/Handmade-Math

Enjoy!
