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

#include "rgmui/rgmuimain.h"

void rgms::rgmui::InitializeDefaultLogger(const std::string& name) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(name + ".log", true);
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks = {console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
}

void rgms::rgmui::RedirectIO() {
#ifdef _WIN32
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        //AllocConsole();
    }

    FILE* ignore;
    freopen_s(&ignore, "CONIN$", "r", stdin);
    freopen_s(&ignore, "CONOUT$", "w", stdout);
    freopen_s(&ignore, "CONOUT$", "w", stderr);

    std::cout.clear(); std::wcout.clear();
    std::cin.clear();  std::wcin.clear();
    std::cerr.clear(); std::wcerr.clear();

    std::cout << std::endl;
#endif
}

void rgms::rgmui::LogAndDisplayException(const std::string& s) {
    spdlog::error(s);
    std::cerr << s << std::endl;
#ifdef _WIN32
    MessageBox(NULL, s.c_str(), NULL, 0x00000030L);
#endif
}

void rgms::rgmui::LogAndDisplayException(const std::exception& e) {
    std::string error = fmt::format("uncaught exception: '{}'", e.what());
    LogAndDisplayException(error);
}

void rgms::rgmui::WindowAppMainLoop(
        Window* window, IApplication* application,
        util::mclock::duration minimumFrameDuration) {
    SDL_Event e;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e) != 0) {
            if (window) {
                running &= window->OnSDLEvent(e);
            }
            if (application) {
                running &= application->OnSDLEventExternal(e);
            }
        }

        auto start = util::Now();
        if (window) {
            window->NewFrame();
        }
        if (application) {
            running &= application->OnFrameExternal();
        }
        if (window) {
            window->EndFrame();
        }
        auto end = util::Now();

        auto elapsed = end - start;
        if (elapsed < minimumFrameDuration) {
            std::this_thread::sleep_for(minimumFrameDuration - elapsed);
        }
    }
}
