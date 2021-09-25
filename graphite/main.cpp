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

#include <fstream>

#include "rgmui/rgmuimain.h"
#include "graphite/graphite.h"
#include "rgmnes/nestopiaimpl.h"

using namespace graphite;
using namespace rgms;

const char* CONFIG_FILE = "graphite.json";

void UpdateDefaultsForScreenSize(GraphiteConfig* config) {
    SDL_DisplayMode displayMode;
    if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0) {
        if (displayMode.w < 1920 || displayMode.h < 1060) {
            config->WindowCfg.WindowWidth = 1440;
            config->WindowCfg.WindowHeight = 810;
            config->WindowCfg.InputSplitLeft = 0.242f;
            config->WindowCfg.LowerSplitDownLeft = 0.30f;
            config->WindowCfg.LowerSplitDownRight = 0.293f;
            config->EmuViewCfg.ScreenPeekCfg.ScreenMultiplier = 2;
            config->VideoCfg.ScreenMultiplier = 2;
        }
    }
}

int main(int argc, char** argv) {
    util::ArgNext(&argc, &argv); // Skip path argument

    int ret = 0;
    try {
        rgmui::InitializeDefaultLogger("graphite");
        spdlog::info("program started");

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error(SDL_GetError());
        }

        GraphiteConfig config = GraphiteConfig::Defaults();
        UpdateDefaultsForScreenSize(&config);

        bool useConfigApp = false;
        if (!ParseArgumentsToConfig(&argc, &argv, CONFIG_FILE, &config)) {
            spdlog::warn("did not parse command line arguments");
            useConfigApp = true;
        }

        rgmui::Window window(config.WindowCfg.WindowWidth, config.WindowCfg.WindowHeight, "Graphite");
        spdlog::info("window created");
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL; 

        if (!config.ImguiIniSettings.empty()) {
            ImGui::LoadIniSettingsFromMemory(config.ImguiIniSettings.c_str());
        }

        bool wasExited = false;

        if (useConfigApp) {
            GraphiteConfigApp cfgApp(&wasExited, &config);
            rgmui::WindowAppMainLoop(&window, &cfgApp, std::chrono::microseconds(10000));
        }

        if (!wasExited) {
            spdlog::info("config completed");
            spdlog::info("ines path: {}", config.InesPath);
            spdlog::info("video path: {}", config.VideoPath);
            spdlog::info("fm2 path: {}", config.FM2Path);

            GraphiteApp app(&config);
            spdlog::info("main loop initiated");
            rgmui::WindowAppMainLoop(&window, &app, std::chrono::microseconds(16333));

            if (config.SaveConfig) {
                config.ImguiIniSettings = std::string(ImGui::SaveIniSettingsToMemory());
                config.WindowCfg.WindowWidth = window.ScreenWidth();
                config.WindowCfg.WindowHeight = window.ScreenHeight();

                std::ofstream of("graphite.json");
                of << std::setw(2) << nlohmann::json(config) << std::endl;
                spdlog::info("config saved");
            }
        }

    } catch(const std::exception& e) {
        rgmui::LogAndDisplayException(e);
        ret = 1;
    } catch (const std::string& s) {
        rgmui::LogAndDisplayException(fmt::format("uncaught string: '{}'", s));
        ret = 1;
    } catch (const char* s) {
        rgmui::LogAndDisplayException(fmt::format("uncaught string: '{}'", s));
        ret = 1;
    } catch (Nes::Result r) {
        rgmui::LogAndDisplayException(fmt::format("uncaught Nes::Result '{}'", rgms::nes::ResultToString(r)));
        ret = 1;
    } catch (int v) {
        rgmui::LogAndDisplayException(fmt::format("uncaught int '{}'", v));
        ret = 1;
    } catch (...) {
        rgmui::LogAndDisplayException("uncaught something else. :(");
        ret = 1;
    }
    spdlog::info("program ended");
    return ret;
}

#ifdef _WIN32

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
    rgmui::RedirectIO();
    return main(__argc, __argv);
}
#endif
