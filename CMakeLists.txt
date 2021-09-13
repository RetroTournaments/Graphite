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
cmake_policy(SET CMP0012 NEW)

project(Graphite)

if (WIN32)
    find_package(OpenGL REQUIRED)
    if (NOT DEFINED OpenCV_DIR)
        message(FATAL_ERROR "You must specify OpenCV_DIR")
    endif()
    find_package(OpenCV CONFIG REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /O2 /std:c++20")
else()
    find_package(OpenCV REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wno-sign-conversion")
endif()


add_subdirectory(3rd)
include_directories(.)

add_subdirectory(rgmutil)
add_subdirectory(rgmui)
add_subdirectory(rgmnes)
add_subdirectory(rgmvideo)

add_subdirectory(graphite)

