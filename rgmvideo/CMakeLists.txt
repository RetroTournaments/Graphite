################################################################################
##
## Copyright (C) 2021-2021 FlibidyDibidy
##
## This file is part of Graphite.
##
## Graphite is free software; you can redistribute it and#or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## Graphite is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Graphite; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
################################################################################
cmake_minimum_required(VERSION 3.6)
project(rgmvideo)

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-anon-enum-enum-conversion")
endif()

################################################################################
# Video Lib
#   - Library for working with video streams
################################################################################
add_library(rgmvideolib
    video.cpp
)
target_include_directories(rgmvideolib PUBLIC
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(rgmvideolib
    rgmutillib
    fmt::fmt
    ${OpenCV_LIBRARIES}
)
if (UNIX)
    add_library(rgmvideolibextra
        libavimpl.cpp
        v4l2impl.cpp
    )
    target_include_directories(rgmvideolibextra PUBLIC
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(rgmvideolibextra
        v4l1 v4l2 avformat avcodec avutil swscale pthread fmt::fmt
    )
endif()

