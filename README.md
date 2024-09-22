# Graphite

Graphite is a tool for turning recorded NES footage into an sequence of button
presses that can be played back in an emulator.

## Download

[downloads](https://github.com/RetroTournaments/Graphite/releases/)

## Contributing

Graphite is open source under the GPLv2 license. Contributions will be
considered on their merits and how they fit with the overall plan.

Consider discussing things first on the [Discord](https://discord.gg/kpYYyw8B5P)

## Build Instructions

## Linux

#### One time setup

Install dependencies

```
sudo apt install build-essential cmake git ninja-build \
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev \
    mesa-common-dev xorg-dev libopencv-dev
```

#### Build

```
cd ~/repos/
git clone --recurse-submodules https://github.com/RetroTournaments/Graphite.git

cd ~/repos/graphite
mkdir build && cd build
cmake -G "Ninja" ..

ninja -j8
```

The main executable will then be in ~/repos/graphite/build/graphite/graphite

## Windows

#### Initial one time setup

- Install Visual Studio 2019 community edition, other versions might work
- Install cmake for windows: cmake.org/download, add to path for ease
- Install anaconda python distribution, make default / put on path

#### Build opencv (should only need to do this once)

```
cd C:\repos
git clone https://github.com/opencv/opencv.git
```

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

cd C:\repos\opencv
mkdir build64
cd build64
cmake -D CMAKE_BUILD_TYPE=RELEASE -D BUILD_opencv_world=ON -A x64 ..
msbuild INSTALL.vcxproj /p:Configuration=Release
```

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86

cd C:\repos\opencv
mkdir build32
cd build32
cmake -D CMAKE_BUILD_TYPE=RELEASE -D BUILD_opencv_world=ON -A Win32 ..
msbuild INSTALL.vcxproj /p:Configuration=Release
```

#### Build graphite

```
cd C:\repos
git clone --recurse-submodules https://github.com/RetroTournaments/Graphite.git

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
cd C:\repos\graphite

mkdir build64
cd build64
cmake -G "Visual Studio 16 2019" -D OpenCV_DIR="C:/repos/opencv/build64/install" -A x64 ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Release
```

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
cd C:\repos\graphite

mkdir build32
cd build32
cmake -G "Visual Studio 16 2019" -D OpenCV_DIR="C:/repos/opencv/build32/install" -A Win32 ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Release
```
