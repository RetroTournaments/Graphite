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
# Graphite Lib
#   - The library with all the components and such that make graphite a thing
################################################################################
add_library(graphitelib
    graphite.cpp
)
target_link_libraries(graphitelib
    rgmneslib rgmuilib rgmvideolib spdlog::spdlog nfd
)
target_include_directories(graphitelib PUBLIC
    ${nlohmann_json_INCLUDE_DIRS}
)
if (UNIX)
    target_link_libraries(graphitelib
        rgmvideolibextra stdc++fs
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
else()
    add_executable(graphite WIN32
        main.cpp
    )
endif()
target_link_libraries(graphite
    graphitelib
)

