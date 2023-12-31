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
project(rgmui)

if(WIN32)
    if (NOT DEFINED OpenCV_DIR)
        message(FATAL_ERROR "You must specify OpenCV_DIR")
    endif()
    find_package(OpenCV CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
else()
    find_package(OpenCV REQUIRED)
endif()

################################################################################
# RGMUI Lib
#   - Library with useful utilities for creating my style of user interfaces
#     with imgui, event queue communication, opencv integration, etc.
################################################################################
add_library(rgmuilib
    rgmui.cpp
    rgmuimain.cpp
)
target_include_directories(rgmuilib PUBLIC
    ${Imgui_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(rgmuilib
    rgmutillib 3rdimgui 
    SDL2 
    ${OpenCV_LIBRARIES}
    fmt::fmt
    spdlog::spdlog
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

