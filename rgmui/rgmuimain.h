////////////////////////////////////////////////////////////////////////////////
//
// File with helpers for defining a 'main' function
//
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

#ifndef RGMS_RGMUIMAIN_HEADER
#define RGMS_RGMUIMAIN_HEADER

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "rgmui/rgmui.h"

namespace rgms::rgmui {

void InitializeDefaultLogger();
void RedirectIO(); // only does stuff on windows
void LogAndDisplayException(const std::exception& e);
void WindowAppMainLoop(
    Window* window, IApplication* application,
    util::mclock::duration minimumFrameDuration = util::mclock::duration(0));




}

#endif
