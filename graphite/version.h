////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2021-2021 FlibidyDibidy
//
// This file is part of Graphite.
//
// Graphite is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Graphite is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Graphite; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#ifndef GRAPHITE_VERSION_HEADER
#define GRAPHITE_VERSION_HEADER

#include "fmt/core.h"

namespace graphite {

#define GRAPHITE_VERSION_MAJOR 0
#define GRAPHITE_VERSION_MINOR 5

inline std::string GraphiteVersionString() {
    return fmt::format("{:1d}.{:03d}", GRAPHITE_VERSION_MAJOR, GRAPHITE_VERSION_MINOR);
}

}

#endif
