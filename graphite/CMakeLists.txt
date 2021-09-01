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
cmake_minimum_required(VERSION 2.6)
project(graphite)

################################################################################
# NES Lib
#   - For emulating the nintendo entertainment system
#   - No dependencies allowed (other than 3rdnestopia)
################################################################################
add_library(neslib
    nes nestopiaimpl
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
    util
)

################################################################################
# Video Lib
#   - Library for working with video streams
################################################################################
add_library(videolib
    video
)
target_link_libraries(videolib
    utillib
    avformat avcodec avutil swscale pthread
    v4l1 v4l2
    fmt::fmt
)


################################################################################
# RGMUI Lib
#   - Library with useful utilities for creating my style of user interfaces
#     with imgui, event queue communication, opencv integration, etc.
################################################################################
find_package(SDL2       REQUIRED)
find_package(OpenCV     REQUIRED)
find_package(GLEW       REQUIRED)
add_library(rgmuilib
    rgmui
)
target_include_directories(rgmuilib PUBLIC
    ${Imgui_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
)
target_link_libraries(rgmuilib
    pthread utillib 3rdimgui 
    GL dl
    ${SDL2_LIBRARIES}
    ${OpenCV_LIBS}
    ${GLEW_LIBRARIES}
    fmt::fmt
)

################################################################################
# Graphite Lib
#   - The library with all the components and such that make graphite a thing
################################################################################
add_library(graphitelib
    graphite
)
target_link_libraries(graphitelib
    neslib rgmuilib videolib
)

################################################################################
# Graphite
#   - The actual executable
################################################################################
add_executable(graphite
    main
)
target_link_libraries(graphite
    graphitelib
)

