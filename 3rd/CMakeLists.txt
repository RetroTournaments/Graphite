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
cmake_minimum_required(VERSION 3.15)
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
    "imgui/backends/imgui_impl_sdl2.cpp"
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
# spdlog
################################################################################
add_subdirectory(spdlog)
set(spdlog_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include/"
    PARENT_SCOPE
)

################################################################################
# nlohmann Json
################################################################################
set(nlohmann_json_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/json/include"
    PARENT_SCOPE
)

#################################################################################
# Native File Dialog Extended
#################################################################################
add_subdirectory(nativefiledialog-extended)
