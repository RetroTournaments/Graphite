Graphite
========

Graphite is a tool for turning recorded NES footage into an sequence of button
presses that can be played back in an emulator. 

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
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev \
```


Install opencv (from source preferred)
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
ninja -j6
```

Windows
-------
#### One time setup
- Install Visual Studio 2019 community edition, other versions might work
- Install cmake for windows: cmake.org/download, add to path for ease
- Download and install [opencv](https://opencv.org/releases/)
    - Click: Windows, Download it, Run the executable
    - I extracted to "C:\opencv"

#### Build
```
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 .. define variables!
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
```


```
msbuild ALL_BUILD.vcxprox
```


gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 wine64 gcc-multilib
x86_64-w64-mingw32-gcc
x86_64-w64-mingw32-g++

cmake -D CMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -D CMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ..
