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

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>

#include "graphite/graphite.h"
#include "graphite/rgmui.h"

using namespace graphite;

int main(int argc, char** argv) {
    util::ArgNext(&argc, &argv); // Skip path argument
    GraphiteConfig config = GraphiteConfig::Defaults();

    rgmui::Window window(1920, 1080, "Graphite");

    if (!ParseArgumentsToConfig(&argc, &argv, &config)) {
        bool wasExited = false;

        GraphiteConfigApp cfgApp(&wasExited, &config);
        rgmui::WindowAppMainLoop(&window, &cfgApp, std::chrono::microseconds(10000));

        if (wasExited) {
            return 0;
        }
    }

    GraphiteApp app(&config);
    rgmui::WindowAppMainLoop(&window, &app, std::chrono::microseconds(10000));
    return 0;
}

#ifdef _WIN32
void RedirectIO() {
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
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
    RedirectIO();
    return main(__argc, __argv);
}
#endif
