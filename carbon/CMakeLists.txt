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
project(carbon)

################################################################################
# Carbon Lib
#   - Components for resizing / morphing input videos into a consistent format
################################################################################
add_library(carbonlib
    carbon.cpp
)
target_link_libraries(carbonlib
    rgmuilib rgmvideolib spdlog::spdlog
)
if (UNIX)
    target_link_libraries(carbonlib
        rgmvideolibextra stdc++fs
    )
endif()

################################################################################
# Carbon
#   - The actual executable for helping define commands to resize videos to be
#     256x240
#   - Also used to apply the uncrt transform to a series of png files
################################################################################
if (UNIX)
    add_executable(carbon
        main.cpp
    )
else()
    add_executable(carbon WIN32
        main.cpp
    )
endif()
target_link_libraries(carbon
    carbonlib
)

