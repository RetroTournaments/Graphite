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
project(graphite)

################################################################################
# NES Lib
#   - For emulating the nintendo entertainment system
#   - No dependencies allowed (other than 3rdnestopia)
################################################################################
add_library(neslib
    nes.cpp 
    nestopiaimpl.cpp
)
target_include_directories(neslib PUBLIC
    ${nestopia_INCLUDE_DIRS}
)
target_link_libraries(neslib
    3rdnestopia
)

################################################################################
# Util Lib
#   - That unfortunate library in c++ projects that contains a wide variety of
#     shared utilities related to time / strings / memory and that nonsense
################################################################################
add_library(utillib
    util.cpp
)

################################################################################
# Video Lib
#   - Library for working with video streams
################################################################################
add_library(videolib
    video.cpp
)
target_link_libraries(videolib
    utillib
    fmt::fmt
)
if (UNIX)
    add_library(videolibextra
        libavimpl.cpp
        v4l2impl.cpp
    )
    target_link_libraries(videolibextra
        v4l1 v4l2 avformat avcodec avutil swscale pthread fmt::fmt
    )
endif()

################################################################################
# RGMUI Lib
#   - Library with useful utilities for creating my style of user interfaces
#     with imgui, event queue communication, opencv integration, etc.
################################################################################
add_library(rgmuilib
    rgmui.cpp
)
target_include_directories(rgmuilib PUBLIC
    ${Imgui_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(rgmuilib
    utillib 3rdimgui 
    SDL2 
    ${OpenCV_LIBRARIES}
    fmt::fmt
)
if (UNIX)
    target_link_libraries(rgmuilib
        pthread GL dl
    )
else()
    target_link_libraries(rgmuilib
        ${OPENGL_LIBRARIES}
    )
endif()

################################################################################
# Graphite Lib
#   - The library with all the components and such that make graphite a thing
################################################################################
add_library(graphitelib
    graphite.cpp
)
target_link_libraries(graphitelib
    neslib rgmuilib videolib 
)
if (UNIX)
    target_link_libraries(rgmuilib
        videolibextra
    )
endif()

################################################################################
# Graphite
#   - The actual executable
################################################################################
if (UNIX)
    add_executable(graphite
        main.cpp
    )
    target_link_libraries(graphite
        graphitelib
    )
else()
    add_executable(graphite WIN32
        main.cpp
    )
    target_link_libraries(graphite
        graphitelib
    )

endif()

