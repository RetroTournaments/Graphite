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

For SDL
```
curl https://www.libsdl.org/release/SDL2-2.0.12.tar.gz --output SDL2-2.0.12.tar.gz
tar -xvf SDL2-2.0.12.tar.gz
cd SDL2-2.0.12.tar.gz
./configure
make all
sudo make install
```

Then build this
```
mkdir build && cd build
cmake ..
```

Windows
-------
Yikes


