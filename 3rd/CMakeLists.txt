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
project(3rd)

################################################################################
# sdl
################################################################################
add_subdirectory(SDL)
set(SDL2_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL/include/"
    PARENT_SCOPE
)
set(SDL2_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL/include/"
)

################################################################################
# Dear ImGui
#   - The user interface library
################################################################################
set(Imgui_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/"
    PARENT_SCOPE
)
add_library(3rdimgui STATIC
    "imgui/imgui.cpp"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_demo.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/imgui_tables.cpp"
    "imgui/backends/imgui_impl_sdl.cpp"
    "imgui/backends/imgui_impl_opengl3.cpp"
)
if (UNIX)
    set_target_properties(3rdimgui PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
target_include_directories(3rdimgui PRIVATE
    "imgui/"
    ${SDL2_INCLUDE_DIRS}
)

################################################################################
# Google test
#   - For unit testing
################################################################################
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

################################################################################
# zlib
################################################################################
add_subdirectory(zlib)

################################################################################
# nestopia
#   - Offload all the complexity of emulating a nintendo entertainment system
################################################################################
add_subdirectory(nestopia)
set(nestopia_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/nestopia/core/"
    "${CMAKE_CURRENT_SOURCE_DIR}/nestopia/core/api"
    PARENT_SCOPE
)

################################################################################
# {fmt}
################################################################################
add_subdirectory(fmt)
set(fmt_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include/"
    PARENT_SCOPE
)

################################################################################
# opencv
################################################################################
add_subdirectory(opencv)
set(OpenCV_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/calib3d/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/core/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/dnn/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/features2d/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/flann/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/gapi/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/highgui/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/imgcodecs/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/imgproc/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/java/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/js/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/ml/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/objc/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/objdetect/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/photo/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/python/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/stitching/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/ts/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/video/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/videoio/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv/modules/world/include/"
    "${CMAKE_BINARY_DIR}"
    PARENT_SCOPE
)
set(OpenCV_LIBRARIES
    opencv_calib3d
    opencv_core
    opencv_dnn
    opencv_features2d
    opencv_flann
    opencv_gapi
    opencv_highgui
    opencv_imgcodecs
    opencv_imgproc
    opencv_ml
    opencv_objdetect
    opencv_photo
    opencv_stitching
    opencv_videoio
    opencv_video
    PARENT_SCOPE
)
