Graphite
========

Graphite is a tool for turning recorded NES footage into an sequence of button
presses that can be played back in an emulator. 

Download
--------
[downloads](https://www.flibidydibidy.com/graphite)

Contributing
------------
Graphite is open source under the GPLv2 license. Contributions will be
considered on their merits and how they fit with the overall plan.

Consider discussing things first on the [Discord](https://discord.gg/kpYYyw8B5P)

Build Instructions
------------------

Obtain the source
```
git clone --recurse-submodules https://gitlab.com/FlibidyDibidy/graphite.git
```

Linux
-----
#### One time setup
Install dependencies (TODO add others)
```
sudo apt install build-essential cmake git \
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev
```


Install opencv
```
cd ~/3rdRepos/
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git

cd opencv
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE \
    -D CMAKE_INSTALL_PREFIX=/usr/local \
    -D INSTALL_C_EXAMPLES=ON \
    -D INSTALL_PYTHON_EXAMPLES=ON \
    -D OPENCV_GENERATE_PKGCONFIG=ON \
    -D OPENCV_EXTRA_MODULES_PATH=~/3rdRepos/opencv_contrib/modules \
    -D BUILD_EXAMPLES=ON ..

make -j8
sudo make install
```

#### Build
```
mkdir build && cd build
cmake -G "Ninja" ..
```

```
ninja -j8
```

Windows
-------
#### Initial one time setup
- Install Visual Studio 2019 community edition, other versions might work
- Install cmake for windows: cmake.org/download, add to path for ease
- Install anaconda python distribution, make default / put on path


#### Build opencv (should only need to do this once)
```
git clone https://github.com/opencv/opencv.git
```

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

cd opencv
mkdir build64
cd build64
cmake -D CMAKE_BUILD_TYPE=RELEASE -D BUILD_opencv_world=ON -A x64 ..
msbuild INSTALL.vcxprox /p:Configuration=Release
```


```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86

cd opencv
mkdir build32
cd build64
cmake -D CMAKE_BUILD_TYPE=RELEASE -D BUILD_opencv_world=ON -A Win32 ..
msbuild INSTALL.vcxprox /p:Configuration=Release

```

#### Build graphite

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
cd graphite

mkdir build64
cd build64
cmake -G "Visual Studio 16 2019" -D OpenCV_DIR="C:/repos/opencv/build64/install" -A x64 ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Release

```

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
cd graphite

mkdir build32
cd build32
cmake -G "Visual Studio 16 2019" -D OpenCV_DIR="C:/repos/opencv/build32/install" -A Win32 ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Release
```