cmake_minimum_required(VERSION 2.6)
project(3rd)

################################################################################
# Dear ImGui
#   - The user interface library
################################################################################
find_package(SDL2       REQUIRED)
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
find_package(ZLIB       REQUIRED) # Used by nestopia for compressing saves/etc
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
