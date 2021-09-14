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

#include <iomanip>
#include <fstream>
#include <iostream>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "fmt/core.h"
#include "imgui_internal.h"
#include "spdlog/spdlog.h"

#include "graphite/graphite.h"
#include "rgmnes/nestopiaimpl.h"


using namespace graphite;
using namespace rgms;

GraphiteConfig GraphiteConfig::Defaults() {
    GraphiteConfig config;
    config.InputsCfg = InputsConfig::Defaults();
    config.EmuViewCfg = EmuViewConfig::Defaults();
    config.VideoCfg = VideoConfig::Defaults();
    config.DoInitialDockspaceSetup = true;
    return config;
}

void graphite::SetFM2PathFromVideoPath(GraphiteConfig* config) {
    fs::path path(config->VideoPath);
    config->FM2Path = fmt::format("{}.fm2", path.stem().string());
}

bool graphite::ParseArgumentsToConfig(int* argc, char*** argv, GraphiteConfig* config) {
    if (*argc == 2) {
        util::ArgReadString(argc, argv, &config->InesPath);
        util::ArgReadString(argc, argv, &config->VideoPath);
        SetFM2PathFromVideoPath(config);
        return true;
    } else if (*argc == 3) {
        util::ArgReadString(argc, argv, &config->InesPath);
        util::ArgReadString(argc, argv, &config->VideoPath);
        util::ArgReadString(argc, argv, &config->FM2Path);
        return true;
    }
    return false;
}

GraphiteApp::GraphiteApp(GraphiteConfig* config) 
    : m_Config(config)
    , m_FirstFrame(true)
{
    RegisterComponent(std::make_shared<NESEmulatorComponent>(
                &m_EventQueue, m_Config->InesPath, &m_Config->EmuViewCfg));
    spdlog::info("registered NESEmulatorComponent");
    RegisterComponent(std::make_shared<InputsComponent>(
                &m_EventQueue, m_Config->FM2Path, &m_Config->InputsCfg));
    spdlog::info("registered InputsComponent");
    RegisterComponent(std::make_shared<VideoComponent>(
                &m_EventQueue, m_Config->VideoPath, &m_Config->VideoCfg));
    spdlog::info("registered VideoComponent");
    RegisterComponent(std::make_shared<PlaybackComponent>(
                &m_EventQueue));
    spdlog::info("registered PlaybackComponent");
}

GraphiteApp::~GraphiteApp() {
}

void GraphiteApp::SetupDockSpace() {
    spdlog::info("using default dock configuration");
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID dockspaceId = rgmui::IApplication::GetDefaultDockspaceID();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

    ImGuiID inputNode = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.18f, nullptr, &dockspaceId);
    ImGuiID screenNode = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.5f, nullptr, &dockspaceId);
    ImGuiID playbackNode = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.25f, nullptr, &dockspaceId);

    ImGui::DockBuilderDockWindow(InputsComponent::WindowName().c_str(), inputNode);
    ImGui::DockBuilderDockWindow(ScreenPeekSubComponent::WindowName().c_str(), screenNode);
    ImGui::DockBuilderDockWindow(VideoComponent::WindowName().c_str(), dockspaceId);
    ImGui::DockBuilderDockWindow(PlaybackComponent::WindowName().c_str(), playbackNode);

    ImGui::DockBuilderFinish(dockspaceId);
}

bool GraphiteApp::OnFrame() {
    if (m_FirstFrame) {
        if (m_Config->DoInitialDockspaceSetup) {
            SetupDockSpace();
        }
        m_FirstFrame = false;
    }
    if (!DoMainMenuBar()) {
        return false;
    }
    m_EventQueue.PumpQueue();
    return true;
}

bool GraphiteApp::DoMainMenuBar() {
    bool ret = true;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("file")) {
            if (ImGui::MenuItem("Save", "Ctrl+s")) {
                m_EventQueue.Publish(EventType::REQUEST_SAVE);
            }
            if (ImGui::MenuItem("Exit", "Ctrl+w")) {
                ret = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("config")) {
            int size = m_Config->VideoCfg.ScreenMultiplier;
            ImGui::PushItemWidth(68);
            if (ImGui::SliderInt("Screen size", &size, 1, 5)) {
                m_Config->VideoCfg.ScreenMultiplier = size;
                m_Config->EmuViewCfg.ScreenPeekCfg.ScreenMultiplier = size;
                m_EventQueue.Publish(EventType::REFRESH_CONFIG);
            }
            ImGui::PopItemWidth();
            ImGui::Checkbox("Sticky auto-scroll", &m_Config->InputsCfg.StickyAutoScroll);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
    return ret;
}

NESEmulatorComponent::NESEmulatorComponent(rgmui::EventQueue* queue, 
        const std::string& inesPath,
        EmuViewConfig* emuViewConfig) 
    : m_EventQueue(queue)
    , m_EmulatorFactory(InitializeEmulatorFactory(inesPath))
    , m_StateSequenceThread(
            emuViewConfig->StateSequenceThreadCfg,
            std::move(m_EmulatorFactory->GetEmu()))
{
    m_EventQueue->SubscribeI(EventType::INPUT_TARGET_SET_TO, [&](int v){
        m_StateSequenceThread.TargetChange(v);
    });
    m_EventQueue->Subscribe(EventType::INPUT_SET_TO, [&](const rgmui::Event& e){
        const InputChangeEvent& v = *reinterpret_cast<InputChangeEvent*>(e.Data.get());
        m_StateSequenceThread.InputChange(v.FrameIndex, v.NewState);
    });

    RegisterSubComponent(std::make_shared<EmuViewComponent>(queue,
                std::move(m_EmulatorFactory->GetEmu()),
                emuViewConfig));
}

nes::NESEmulatorFactorySPtr NESEmulatorComponent::InitializeEmulatorFactory(const std::string& inesPath) {
    auto factory = std::make_shared<nes::NESEmulatorFactory>(
        [&](){
            return std::move(std::make_unique<nes::NestopiaNESEmulator>());
        });

    std::ifstream ifs(inesPath, std::ios::binary);
    if (!ifs.good()) {
        std::ostringstream os;
        os << "Unable to open INES file '" << inesPath << "'";
        throw std::runtime_error(os.str());
    }

    std::stringstream b;
    b << ifs.rdbuf();
    factory->SetDefaultINESString(b.str());
    return factory;
}

NESEmulatorComponent::~NESEmulatorComponent() {
}

void NESEmulatorComponent::OnFrame() {
    int frameIndex;
    std::string nesState;
    if (m_StateSequenceThread.HasNewState(&frameIndex, &nesState)) {
        m_EventQueue->PublishI(EventType::NES_FRAME_SET_TO, frameIndex);
        m_EventQueue->PublishS(EventType::NES_STATE_SET_TO, nesState);
    }
    OnSubComponentFrames();
}

////////////////////////////////////////////////////////////////////////////////

//   ColumnPadding
// |  |      |                                
//  >> 000001 aaaabbbbssssttttuuuuddddllllrrrr
//  ||      |    |   |   |   |   |   |   |   |
//  ||      |    |___|___|___|___|___|___|___|_ the button states
//  ||      |_ the FrameText
//  ||_ the 'target' cursor
//  |_the emulator cursor
//  __the ChevronColumnWidth

enum TASEditorInputsColumnIndices : int {
    CHEVRON = 0,
    FRAME_TEXT,
    BUTTONS,
};

InputsConfig InputsConfig::Defaults() {
    InputsConfig cfg;
    cfg.ColumnPadding = 4;
    cfg.ChevronColumnWidth = 18;
    cfg.FrameTextColumnWidth = 40;
    cfg.ButtonWidth = 29;
    cfg.FrameTextNumDigits = 6;
    cfg.MaxInputSize = 40000;
    cfg.ScrollOffset = 10;
    cfg.TextColor = IM_COL32(255, 255, 255, 128);
    cfg.HighlightTextColor = IM_COL32_WHITE;
    cfg.ButtonColor = IM_COL32(215, 25, 25, 255);
    cfg.StickyAutoScroll = true;

    return cfg;
}

InputsComponent::InputsComponent(rgmui::EventQueue* queue,
        const std::string& fm2Path,
        InputsConfig* config)
    : m_EventQueue(queue)
    , m_TargetScroller(this)
    , m_FM2Path(fm2Path)
    , m_Config(config)
    , m_UndoRedo(queue, &m_Inputs)
    , m_Inputs(1000, 0)
    , m_AllowDragging(true)
    , m_TargetDragging(false)
    , m_LockTarget(0)
    , m_CouldToggleLock(false)
    , m_TargetIndex(0)
    , m_CurrentIndex(0)
{
    m_Drag.Clear();
    queue->SubscribeI(EventType::SET_INPUT_TARGET_TO, [&](int v){
        ChangeTargetTo(v, false);
    });
    queue->SubscribeI(EventType::SCROLL_INPUT_TARGET, [&](int v){
        int q = m_TargetIndex + v;
        if (q < 0) {
            q = 0;
        }
        if (q >= static_cast<int>(m_Inputs.size())) {
            q = static_cast<int>(m_Inputs.size()) - 1;
        }

        if (q != m_TargetIndex) {
            ChangeTargetTo(q, false);
        }
    });
    queue->SubscribeI(EventType::NES_FRAME_SET_TO, [&](int v){
        m_CurrentIndex = v;
    });
    queue->SubscribeI(EventType::OFFSET_SET_TO, [&](int v){
        m_OffsetMillis = v;
    });
    queue->Subscribe(EventType::REQUEST_SAVE, [&](){
        WriteFM2();
    });

    TryReadFM2();
}

InputsComponent::~InputsComponent() {
    WriteFM2();
}

static bool StringStartsWith(const std::string& str, const std::string& start) {
    if (str.length() >= start.length()) {
        return str.compare(0, start.length(), start) == 0;
    }
    return false;
}

void InputsComponent::TryReadFM2() {
    std::ifstream ifs(m_FM2Path);
    if (ifs.good()) {
        nes::ReadFM2File(ifs, &m_Inputs, &m_Header);
        int frameIndex = 0;
        for (auto & input : m_Inputs) {
            if (input) {
                m_EventQueue->Publish(EventType::INPUT_SET_TO, 
                        std::make_shared<InputChangeEvent>(frameIndex, input));
            }
            frameIndex++;
        }
        for (auto & line : m_Header.additionalLines) {
            if (StringStartsWith(line, FM2_OFFSET_COMMENT_LINE_START)) {
                int v = std::stoi(line.substr(FM2_OFFSET_COMMENT_LINE_START.size()));
                m_EventQueue->PublishI(EventType::SET_OFFSET_TO, v);
                break;
            }
        }

    } else {
        m_Header = nes::FM2Header::Defaults();
    }
}

std::string InputsComponent::OffsetLine() const {
    return fmt::format("{}{}", FM2_OFFSET_COMMENT_LINE_START, m_OffsetMillis);
}

void InputsComponent::WriteFM2() {
    std::ofstream ofs(m_FM2Path);
    bool offsetFound = false;
    for (auto & line : m_Header.additionalLines) {
        if (StringStartsWith(line, FM2_OFFSET_COMMENT_LINE_START)) {
            line = OffsetLine();
            offsetFound = true;
        }
    }
    if (!offsetFound) {
        m_Header.additionalLines.push_back(OffsetLine());
    }
    while (m_Inputs.size() > 1000 && m_Inputs.back() == 0) {
        m_Inputs.pop_back();
    }
    if (m_Inputs.back() != 0) {
        for (int i = 0; i < 10; i++) {
            m_Inputs.push_back(0);
        }
    }

    spdlog::info("written fm2 to '{}' sync offset: '{}'", m_FM2Path, m_OffsetMillis);
    nes::WriteFM2File(ofs, m_Inputs, m_Header);
}

std::string InputsComponent::FrameText(int frameId) const {
    std::ostringstream os;
    os << std::setw(m_Config->FrameTextNumDigits) << std::setfill('0') << frameId + 1;
    return os.str();
}

ImVec2 InputsComponent::CalcLineSize() {
    int height = ImGui::GetFrameHeight() + 3;

    int width = 0;
    width += m_Config->ColumnPadding;
    m_ColumnX.push_back(width); // CHEVRON
    width += m_Config->ChevronColumnWidth;
    width += m_Config->ColumnPadding;
    m_ColumnX.push_back(width); // FRAME_TEXT
    width += m_Config->FrameTextColumnWidth;
    width += m_Config->ColumnPadding;
    m_ColumnX.push_back(width); // BUTTONS
    width += m_Config->ButtonWidth * 8;
    width += m_Config->ColumnPadding;

    return ImVec2(width, height);
}

std::string InputsComponent::ButtonText(uint8_t button) {
    std::string bt = "A";
    if (button == nes::Button::B) {
        bt = "B";
    } else if (button == nes::Button::SELECT) {
        bt = "S";
    } else if (button == nes::Button::START) {
        bt = "T";
    } else if (button == nes::Button::UP) {
        bt = "U";
    } else if (button == nes::Button::DOWN) {
        bt = "D";
    } else if (button == nes::Button::LEFT) {
        bt = "L";
    } else if (button == nes::Button::RIGHT) {
        bt = "R";
    }
    return bt;
}

void InputsComponent::ChangeTargetTo(int frameIndex, bool byChevronColumn) {
    if (frameIndex < 0) {
        frameIndex = 0;
    }
    if (m_LockTarget != -1) {
        m_LockTarget = frameIndex;
    }
    if (frameIndex != m_TargetIndex) {
        m_TargetIndex = frameIndex;
        m_EventQueue->PublishI(EventType::INPUT_TARGET_SET_TO, m_TargetIndex);
        m_TargetScroller.OnTargetChange(byChevronColumn);
    }
}

void InputsComponent::DrawButton(ImDrawList* list, ImVec2 ul, ImVec2 lr, uint8_t button, bool buttonOn, bool highlighted) {
    float rounding = (lr.y - ul.y) * 0.2;
    float shrink = rounding * 0.5;

    ImVec2 ul2(ul.x + shrink, ul.y + shrink);
    ImVec2 lr2(lr.x - shrink, lr.y - shrink);

    if (highlighted) {
        list->AddRect(ul, lr, IM_COL32_WHITE, rounding, 0, 2.0f);
    }

    if (buttonOn) {
        list->AddRectFilled(ul2, lr2, m_Config->ButtonColor, rounding);
    }
    std::string buttonText = ButtonText(button);
    ImVec2 txtsiz = ImGui::CalcTextSize(buttonText.c_str());
    ImVec2 txtpos(ul.x + ((lr.x - ul.x) - txtsiz.x) / 2, ul.y + 1);
    list->AddText(txtpos, TextColor(highlighted), buttonText.c_str());
}

ImU32 InputsComponent::TextColor(bool highlighted) {
    return highlighted ? m_Config->HighlightTextColor : m_Config->TextColor;
}

void InputsComponent::DoInputLine(int frameIndex) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* list = ImGui::GetWindowDrawList();

    ImGui::PushID(frameIndex);
    ImGui::InvisibleButton("input", m_LineSize);
    bool textHighlighted = m_AllowDragging && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
    if (m_Drag.IsDragging && textHighlighted) {
        int s = frameIndex;
        int e = m_Drag.LastHighlighted;
        if (s > e) {
            std::swap(s, e);
        }
        for (int i = s; i <= e; i++) {
            if (m_Drag.Button == 0) {
                ChangeInputTo(i, m_Drag.HLButtons);
            } else {
                if (m_Drag.HighlightedFrames.size() > 1) {
                    //ChangeButtonTo(m_Drag.StartFrameIndex, m_Drag.HLButtons, !m_Drag.StartedOn);
                    ChangeButtonTo(i, m_Drag.HLButtons, !m_Drag.StartedOn);
                } else {
                    if ((e - s) > 0) {
                        ChangeButtonTo(m_Drag.StartFrameIndex, m_Drag.HLButtons, !m_Drag.StartedOn);
                        ChangeButtonTo(i, m_Drag.HLButtons, !m_Drag.StartedOn);
                    }
                }
            }
        }
        m_Drag.LastHighlighted = frameIndex;
    }

    ImVec2 dragDeltal = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
    ImVec2 gul(p.x, p.y);
    ImVec2 glr(p.x + m_ColumnX[TASEditorInputsColumnIndices::FRAME_TEXT], p.y + m_LineSize.y);
    bool targetDraggingAndGood = false;
    if (m_TargetDragging) {
        ImVec2 mp = ImGui::GetMousePos();
        if (mp.y >= gul.y && mp.y < glr.y) {
            targetDraggingAndGood = true;
        }
    }

    // Clicks in chevron column
    if (m_AllowDragging &&
        (targetDraggingAndGood || ImGui::IsMouseHoveringRect(gul, glr)) && 
        !m_Drag.IsDragging && 
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {

        m_TargetScroller.SuspendAutoScroll();

        if (!m_TargetDragging) {
            if (frameIndex == m_TargetIndex) {
                m_CouldToggleLock = true;
            } else {
                m_CouldToggleLock = false;
            }
        }

        m_TargetDragging = true;
        ChangeTargetTo(frameIndex, true);
    }
    if (m_CouldToggleLock && m_TargetDragging &&
        dragDeltal.x == 0 && dragDeltal.y == 0 &&
        ImGui::IsMouseHoveringRect(gul, glr) &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)
        ) {

        if (frameIndex == m_TargetIndex) {
            if (m_LockTarget != frameIndex) {
                m_LockTarget = frameIndex;
            } else {
                m_LockTarget = -1;
            }
        }
    }

    if (m_TargetIndex == frameIndex || m_CurrentIndex == frameIndex) {
        ImVec2 a = gul;
        ImVec2 b(glr.x - 3, p.y + m_LineSize.y / 2);
        ImVec2 c(p.x, p.y + m_LineSize.y);
        if (m_TargetIndex == frameIndex) {
            list->AddTriangleFilled(a, b, c, m_Config->ButtonColor);
        }
        if (m_CurrentIndex == frameIndex) {
            list->AddTriangle(a, b, c, IM_COL32_WHITE, 1.0f);
        }
        if (m_LockTarget == frameIndex) {
            ImVec2 d((a.x * 0.7 + b.x * 0.3), b.y);
            list->AddCircleFilled(d, m_LineSize.y / 4, IM_COL32_WHITE);
        }
    }


    ImVec2 tul(p.x + m_ColumnX[TASEditorInputsColumnIndices::FRAME_TEXT], p.y);
    ImVec2 tlr(p.x + m_ColumnX[TASEditorInputsColumnIndices::BUTTONS], p.y + m_LineSize.y);

    ImVec2 txtpos(tul.x, p.y + 1);
    textHighlighted |= ImGui::IsPopupOpen("frame_popup");
    bool inHighlightList = (m_Drag.IsDragging && m_Drag.HighlightedFrames.find(frameIndex) != m_Drag.HighlightedFrames.end());
    textHighlighted |= inHighlightList;
    list->AddText(txtpos, TextColor(textHighlighted), FrameText(frameIndex).c_str());

    if (!rgmui::IsAnyPopupOpen() && ImGui::IsMouseHoveringRect(tul, tlr)) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_AllowDragging) {
            m_Drag.StartDrag(frameIndex, 0x00, m_Inputs[frameIndex], false);
        }
    }


    uint8_t thisInput = m_Inputs[frameIndex];

    int bx = m_ColumnX[TASEditorInputsColumnIndices::BUTTONS];
    int buttonCount = 0;
    for (uint8_t button = 0x01; button != 0; button <<= 1) {
        bool buttonOn = thisInput & button;
        bool highlighted = inHighlightList && (button & m_Drag.HLButtons);


        int tx = p.x + bx + buttonCount * m_Config->ButtonWidth;
        ImVec2 ul(tx, p.y);
        ImVec2 lr(tx + m_Config->ButtonWidth, p.y + m_LineSize.y);

        if (m_AllowDragging && !rgmui::IsAnyPopupOpen() && ImGui::IsMouseHoveringRect(ul, lr)) {
            if (!inHighlightList) {
                highlighted = true;
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_Drag.StartDrag(frameIndex, button, button, buttonOn);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (m_Drag.IsDragging && m_Drag.StartFrameIndex == frameIndex &&
                        m_Drag.Button == button) {
                    if (buttonOn) {
                        thisInput &= ~button;
                    } else {
                        thisInput |= button;
                    }

                    ChangeInputTo(frameIndex, thisInput);
                }
            }
        }

        if (m_TargetDragging) {
            highlighted = false;
        }

        if (buttonOn || highlighted) {
            DrawButton(list, ul, lr, button, buttonOn, highlighted);
        }

        buttonCount++;
    }

    ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && 
        dragDelta.x == 0.0f &&
        dragDelta.y == 0.0f) {
        ImGui::OpenPopupOnItemClick("frame_popup");
    }
    if (ImGui::BeginPopup("frame_popup")) {
        if (ImGui::MenuItem(fmt::format("clear {}", frameIndex).c_str())) {
            ChangeInputTo(frameIndex, 0x00);
        }

        ImGui::EndPopup();
    }



    ImGui::PopID();
}

void InputsComponent::ChangeInputTo(int frameIndex, nes::ControllerState newState) {
    if (m_LockTarget == -1) {
        ChangeTargetTo(frameIndex, false);
    }
    if (m_Drag.HighlightedFrames.find(frameIndex) == m_Drag.HighlightedFrames.end()) {
        m_UndoRedo.ChangeInputTo(frameIndex, newState);
        m_Drag.HighlightedFrames.insert(frameIndex);
    }
}
void InputsComponent::ChangeButtonTo(int frameIndex, uint8_t button, bool onoff) {
    uint8_t input = m_Inputs[frameIndex];
    if (onoff) {
        input |= button;
    } else {
        input &= ~button;
    }
    ChangeInputTo(frameIndex, input);
}

void InputsComponent::DoInputList(ImVec2 p, int startIndex, int endIndex) {
    m_LineSize = CalcLineSize();
    ImGui::SetCursorScreenPos(p);

    for (int i = startIndex; i < endIndex; i++) {
        DoInputLine(i);
    }
}

void InputsComponent::DragInfo::StartDrag(int frameIndex, uint8_t button, uint8_t hlButtons, bool on) {
    IsDragging = true;
    StartFrameIndex = frameIndex;
    Button = button;
    HLButtons = hlButtons;
    LastHighlighted = frameIndex;
    StartedOn = on;
}

void InputsComponent::DragInfo::Clear() {
    IsDragging = false;
    StartFrameIndex = -1;
    Button = 0x00; 
    HLButtons = 0x00;
    StartedOn = false;
    HighlightedFrames.clear();
    LastHighlighted = -1;
}

void InputsComponent::OnSDLEvent(const SDL_Event& e) {
    auto& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (rgmui::KeyDownWithCtrl(e, SDLK_z)) {
            m_UndoRedo.Undo();
        } else if (rgmui::KeyDownWithCtrl(e, SDLK_y)) {
            m_UndoRedo.Redo();
        }
        else if (rgmui::KeyDownWithCtrl(e, SDLK_s)) {
            WriteFM2();
        }
    }
}

std::string InputsComponent::WindowName() {
    return "Inputs";
}

void InputsComponent::DoMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("inputs")) {
            if (ImGui::MenuItem("Undo", "Ctrl+z")) {
                m_UndoRedo.Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+y")) {
                m_UndoRedo.Redo();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void InputsComponent::OnFrame() {
    DoMainMenuBar();

    if (ImGui::Begin(WindowName().c_str())) {
        m_AllowDragging = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_TargetScroller.DoButtons();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        ImGui::BeginChild("buttons", ImVec2(0, 0), true, ImGuiWindowFlags_NoMove);
        m_TargetScroller.UpdateScroll();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_Inputs.size()));
        while (clipper.Step()) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            DoInputList(p, clipper.DisplayStart, clipper.DisplayEnd);
        }

        if (ImGui::GetScrollY() == ImGui::GetScrollMaxY()) {
            if (m_Inputs.size() < m_Config->MaxInputSize) {
                m_Inputs.insert(m_Inputs.end(), 100, 0x00);
            }
            if (m_Inputs.size() > m_Config->MaxInputSize) {
                m_Inputs.resize(m_Config->MaxInputSize);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (!m_Drag.HighlightedFrames.empty()) {
                m_UndoRedo.ConsolidateLast(
                        static_cast<int>(m_Drag.HighlightedFrames.size()));
            }
            m_Drag.Clear();
            m_TargetDragging = false;
            m_CouldToggleLock = false;
        }
    }
    ImGui::End();
}

InputsComponent::TargetScroller::TargetScroller(InputsComponent* inputs)
    : m_InputsComponent(inputs)
    , m_AutoScroll(true)
    , m_AutoScrollWasSetOnByUser(true)
    , m_TargetScrollY(-1.0f)
    , m_CurrentMaxY(0)
    , m_VisibleY(0)
    , m_LastScrollY(0)
    , m_LastScrollTarget(0)
{
}

InputsComponent::TargetScroller::~TargetScroller() {
}

void InputsComponent::TargetScroller::DoButtons() {
    if (ImGui::Button("scroll to target")) {
        SetScrollDirectTo(m_InputsComponent->m_TargetIndex);
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("auto-scroll", &m_AutoScroll)) {
        if (m_AutoScroll) {
            SetScrollDirectTo(m_InputsComponent->m_TargetIndex);
        }
        m_AutoScrollWasSetOnByUser = m_AutoScroll;
    }
}

void InputsComponent::TargetScroller::UpdateScroll() {
    if (!m_ScrolledNext && (ImGui::GetScrollY() != m_LastScrollY)) {
        m_AutoScroll = false; // User scrolled by themselves
    }
    m_ScrolledNext = false;
    m_CurrentMaxY = ImGui::GetScrollMaxY();
    m_VisibleY = ImGui::GetContentRegionAvail().y;
    
    if (m_AutoScroll && (m_InputsComponent->m_TargetIndex != m_LastScrollTarget)) {
        SetScrollDirectTo(m_InputsComponent->m_TargetIndex);
    }

    if (m_TargetScrollY >= 0.0f) {
        ImGui::SetScrollY(m_TargetScrollY);
        m_TargetScrollY = -1.0f;
        m_ScrolledNext = true;
    }
    m_LastScrollY = ImGui::GetScrollY();
}

float InputsComponent::TargetScroller::TargetY(int target) {
    float y = static_cast<float>(target) * (m_CurrentMaxY + m_VisibleY) / m_InputsComponent->m_Inputs.size() - m_VisibleY / 2;
    if (y < 0.0f) {
        y = 0.0f;
    }
    if (y > m_CurrentMaxY) {
        y = m_CurrentMaxY;
    }
    return y;
}

void InputsComponent::TargetScroller::SetScrollDirectTo(int target) {
    m_TargetScrollY = TargetY(target);
    m_LastScrollTarget = target;
}

void InputsComponent::TargetScroller::SuspendAutoScroll() {
    m_AutoScroll = false;
}

void InputsComponent::TargetScroller::OnTargetChange(bool byChevronColumn) {
    if (!byChevronColumn && m_AutoScrollWasSetOnByUser && m_InputsComponent->m_Config->StickyAutoScroll) {
        m_AutoScroll = true;
    }
}

////////////////////////////////////////////////////////////////////////////////

UndoRedo::UndoRedo(rgmui::EventQueue* queue,
        std::vector<nes::ControllerState>* inputs)
    : m_EventQueue(queue)
    , m_Inputs(inputs)
    , m_ChangeIndex(0)
{
}

UndoRedo::~UndoRedo() {
}

UndoRedo::Change::Change()  {
}

UndoRedo::Change::Change(int _frameIndex, nes::ControllerState _oldState, nes::ControllerState _newState) 
    : FrameIndex(_frameIndex)
    , OldState(_oldState)
    , NewState(_newState)
    , Consolidated(false)
{
}

void UndoRedo::ChangeInputTo(int frameIndex, nes::ControllerState newState) {
    m_Changes.resize(m_ChangeIndex);
    m_Changes.emplace_back(frameIndex, m_Inputs->at(frameIndex), newState);

    IntChangeInput(frameIndex, newState);

    m_ChangeIndex++;
}

void UndoRedo::IntChangeInput(int frameIndex, nes::ControllerState newState) {
    m_Inputs->at(frameIndex) = newState;
    m_EventQueue->Publish(EventType::INPUT_SET_TO, 
            std::make_shared<InputChangeEvent>(frameIndex, newState));

}

void UndoRedo::Undo() {
    if (m_ChangeIndex > 0) {
        do {
            m_ChangeIndex--;
            assert(m_ChangeIndex >= 0);


            IntChangeInput(m_Changes[m_ChangeIndex].FrameIndex,
                    m_Changes[m_ChangeIndex].OldState);
        } while (m_Changes[m_ChangeIndex].Consolidated);
    }
}

void UndoRedo::Redo() {
    if (m_ChangeIndex < m_Changes.size()) {
        do {
            IntChangeInput(m_Changes[m_ChangeIndex].FrameIndex,
                    m_Changes[m_ChangeIndex].NewState);

            m_ChangeIndex++;
        } while (m_ChangeIndex < m_Changes.size() && m_Changes[m_ChangeIndex].Consolidated);
    }
}

void UndoRedo::ConsolidateLast(int count) {
    assert(count >= 0);
    assert(count <= m_ChangeIndex);

    int numNoChange = 0;
    int origChangeIndex = m_ChangeIndex;
    for (int i = origChangeIndex - 1; i >= (origChangeIndex - count); i--) {
        if (m_Changes[i].OldState == m_Changes[i].NewState) {
            numNoChange++;
            m_ChangeIndex--;
            std::swap(m_Changes[i], m_Changes[m_ChangeIndex]);
            m_Changes.pop_back();
        }
    }


    count -= numNoChange;
    if (count > 0) {
        for (int i = (m_ChangeIndex - 1); i > (m_ChangeIndex - count); i--) {
            m_Changes[i].Consolidated = true;
        }
    } 

}

////////////////////////////////////////////////////////////////////////////////

InputChangeEvent::InputChangeEvent(int frameIndex, nes::ControllerState newState)
    : FrameIndex(frameIndex)
    , NewState(newState)
{
}

////////////////////////////////////////////////////////////////////////////////

IEmuPeekSubComponent::IEmuPeekSubComponent() {
}

IEmuPeekSubComponent::~IEmuPeekSubComponent() {
}

EmuViewConfig EmuViewConfig::Defaults() {
    EmuViewConfig cfg;
    cfg.ScreenPeekCfg = ScreenPeekConfig::Defaults();
    cfg.StateSequenceThreadCfg = nes::StateSequenceThreadConfig::Defaults();
    return cfg;
}

EmuViewComponent::EmuViewComponent(rgmui::EventQueue* queue,
            std::unique_ptr<nes::INESEmulator>&& emu,
            EmuViewConfig* config)
    : m_EventQueue(queue)
    , m_Emulator(std::move(emu))
{
    RegisterEmuPeekComponent(std::make_shared<ScreenPeekSubComponent>(queue, &config->ScreenPeekCfg));

    m_EventQueue->SubscribeS(NES_STATE_SET_TO, [&](const std::string& state){
        if (state.empty()) {
            for (auto & comp : m_EmuComponents) {
                comp->CacheNewEmulatorData(nullptr);
            }
        } else {
            m_Emulator->LoadStateString(state);
            for (auto & comp : m_EmuComponents) {
                comp->CacheNewEmulatorData(m_Emulator.get());
            }
        }
    });

}

EmuViewComponent::~EmuViewComponent() {
}

void EmuViewComponent::OnFrame() {
    OnSubComponentFrames();
}

void EmuViewComponent::RegisterEmuPeekComponent(std::shared_ptr<IEmuPeekSubComponent> component) {
    RegisterSubComponent(component);
    m_EmuComponents.push_back(component);
}

////////////////////////////////////////////////////////////////////////////////

ScreenPeekConfig ScreenPeekConfig::Defaults() {
    ScreenPeekConfig cfg;
    cfg.ScreenMultiplier = 3;
    cfg.NESPalette = nes::DefaultPalette();
    return cfg;
}

ScreenPeekSubComponent::ScreenPeekSubComponent(rgmui::EventQueue* queue,
        ScreenPeekConfig* config)
    : m_EventQueue(queue)
    , m_Config(config)
{
    m_EventQueue->Subscribe(EventType::REFRESH_CONFIG, [&](){
        ConstructImageFromFrame();
    });
    SetBlankImage();
}

ScreenPeekSubComponent::~ScreenPeekSubComponent() {
}

void ScreenPeekSubComponent::CacheNewEmulatorData(nes::INESEmulator* emu) {
    if (emu) {
        emu->ScreenPeekFrame(&m_Frame);
        ConstructImageFromFrame();
    } else {
        SetBlankImage();
    }
}

void ScreenPeekSubComponent::ConstructImageFromFrame() {
    m_Image = rgmui::ConstructPaletteImage(
            m_Frame.data(), nes::FRAME_WIDTH, nes::FRAME_HEIGHT,
            m_Config->NESPalette.data());
    cv::resize(m_Image, m_Image, {}, m_Config->ScreenMultiplier, m_Config->ScreenMultiplier,
            cv::INTER_NEAREST);
}

void ScreenPeekSubComponent::SetBlankImage() {
    m_Frame.fill(0);
    m_Image = cv::Mat::zeros(
            nes::FRAME_HEIGHT * m_Config->ScreenMultiplier,
            nes::FRAME_WIDTH * m_Config->ScreenMultiplier, CV_8UC3);
}


std::string ScreenPeekSubComponent::WindowName() {
    return "Screen";
}

void ScreenPeekSubComponent::OnFrame() {
    if (ImGui::Begin(WindowName().c_str())) {
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        rgmui::Mat("screen", m_Image);
        if (ImGui::IsItemHovered()) {
            auto& io = ImGui::GetIO();
            int m = static_cast<int>(-io.MouseWheel);

            if (m) {
                m_EventQueue->PublishI(EventType::SCROLL_INPUT_TARGET, m);
            }
        }

        //int x, y;
        //uint8_t pixel;

        //ImGui::OpenPopupOnItemClick("screen_popup");
        //if (ImGui::BeginPopup("screen_popup")) {
        //    ImVec2 m = ImGui::GetMousePosOnOpeningCurrentPopup();
        //    if (GetScreenPeekPixel(cursorPos, ImGui::GetMousePosOnOpeningCurrentPopup(),
        //                &x, &y, &pixel)) {
        //        DoPopupItems(x, y, pixel);
        //    }
        //    ImGui::EndPopup();
        //} else {
        //    if (ImGui::IsWindowFocused()) {
        //        if (GetScreenPeekPixel(cursorPos, ImGui::GetMousePos(), 
        //                    &x, &y, &pixel)) {
        //            DoHoverTooltip(x, y, pixel);
        //        }
        //    }
        //}
    }
    ImGui::End();
}

std::string ScreenPeekSubComponent::PositionText(int x, int y) const {
    return fmt::format("{{{:3d}, {:3d}}}", x, y);
}
std::string ScreenPeekSubComponent::PixelText(uint8_t pixel) const {
    return fmt::format("0x{:02x}", pixel);
}

ImVec4 ScreenPeekSubComponent::PeekPixelColor(uint8_t pixel) const {
    ImVec4 c(0, 0, 0, 1.0);
    if (pixel < nes::PALETTE_ENTRIES) {
        c.x = static_cast<float>(m_Config->NESPalette[pixel * 3 + 0]) / 255.0f;
        c.y = static_cast<float>(m_Config->NESPalette[pixel * 3 + 1]) / 255.0f;
        c.z = static_cast<float>(m_Config->NESPalette[pixel * 3 + 2]) / 255.0f;
    }
    return c;
}

bool ScreenPeekSubComponent::GetScreenPeekPixel(const ImVec2& cursorPos, const ImVec2& mousePos,
        int* x, int* y, uint8_t* pixel) const {

    int ix = std::round((mousePos.x - cursorPos.x) / m_Config->ScreenMultiplier);
    int iy = std::round((mousePos.y - cursorPos.y) / m_Config->ScreenMultiplier);

    if (ix >= 0 && ix < nes::FRAME_WIDTH &&
        iy >= 0 && iy < nes::FRAME_HEIGHT) {

        *pixel = m_Frame[iy * nes::FRAME_WIDTH + ix];
        *x = ix;
        *y = iy;
        return true;
    }
    return false;
}

void ScreenPeekSubComponent::DoPopupItems(int x, int y, uint8_t pixel) {
    std::string pt = PixelText(pixel);
    if (ImGui::MenuItem(fmt::format("copy pixel '{}'", pt).c_str())) {
        ImGui::SetClipboardText(pt.c_str());
    }

    std::string post = PositionText(x, y);
    if (ImGui::MenuItem(fmt::format("copy position '{}'", post).c_str())) {
        ImGui::SetClipboardText(post.c_str());
    }

    if (ImGui::MenuItem("save .png")) {
        std::cout << "not supported yet - need some sort of file dialog" << std::endl;
    }

    if (ImGui::MenuItem("save .frm")) {
        std::cout << "not supported yet - need some sort of file dialog (and to define frame)" << std::endl;
    }

    ImGui::PushItemWidth(100);
    if (ImGui::SliderInt("mult", &m_Config->ScreenMultiplier, 1, 5)) {
        ConstructImageFromFrame();
    }
    ImGui::PopItemWidth();
}

void ScreenPeekSubComponent::DoHoverTooltip(int x, int y, uint8_t pixel) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(PositionText(x, y).c_str());
    ImGui::ColorButton("##palette", PeekPixelColor(pixel));
    ImGui::SameLine();
    ImGui::TextUnformatted(PixelText(pixel).c_str());
    ImGui::EndTooltip();
}

////////////////////////////////////////////////////////////////////////////////

VideoConfig VideoConfig::Defaults() {
    VideoConfig cfg;
    cfg.MaxFrames = 40000;
    cfg.ScreenMultiplier = 3;
    cfg.OffsetMillis = 0;

    return cfg;
}

VideoComponent::VideoComponent(rgmui::EventQueue* queue,
        const std::string& videoPath,
        VideoConfig* videoCfg)
    : m_EventQueue(queue)
    , m_Config(videoCfg)
    , m_CurrentVideoIndex(-1)
    , m_InputTarget(0)
    , m_VideoPath(videoPath)
{
    m_EventQueue->SubscribeI(EventType::INPUT_TARGET_SET_TO, [&](int v){
        m_InputTarget = v;
        FindTargetFrame(v);
    });
    m_EventQueue->SubscribeI(EventType::SET_OFFSET_TO, [&](int v){
        SetOffset(v);
    });
    m_EventQueue->Subscribe(EventType::REFRESH_CONFIG, [&](){
        SetImageFromInputFrame();
    });

    {
        std::ifstream ifs(videoPath);
        if (!ifs.good()) {
            std::ostringstream os;
            os << "Unable to open Video file '" << videoPath << "'";
            throw std::runtime_error(os.str());
        }
    }

    m_VideoThread = std::make_unique<video::LiveVideoThread>(
                std::make_unique<video::CVVideoCaptureSource>(videoPath),
                m_Config->MaxFrames, false);

    if (m_VideoThread->HasError()) {
        throw std::runtime_error(m_VideoThread->GetError());
    }
    spdlog::info("inputs information: {}", m_VideoThread->InputInformation());
    SetBlankImage();
}


VideoComponent::~VideoComponent() {
}

void VideoComponent::UpdatePTSs() {
    int n = static_cast<int>(m_VideoThread->QueueSize());
    int pn = static_cast<int>(m_PTS.size());
    if (n != pn) {
        if (pn > n) {
            m_PTS.clear();
            pn = 0;
        }

        m_PTS.resize(n);
        for (int i = pn; i < n; i++) {
            auto f = m_VideoThread->GetFrame(i);
            assert(f);
            m_PTS[i] = f->PtsMilliseconds;
        }

        assert(std::is_sorted(m_PTS.begin(), m_PTS.end()));
    }
}

int64_t VideoComponent::FrameIndexToPTS(int frameIndex) {
    double fi = static_cast<double>(frameIndex);
    double denom = static_cast<double>(nes::NTSC_FPS_DENOMINATOR);
    double numer = static_cast<double>(nes::NTSC_FPS_NUMERATOR);

    int64_t targetPts = static_cast<int64_t>(std::round((fi * denom * 1000) / numer)) + m_Config->OffsetMillis;
    return targetPts;
}

int VideoComponent::PTSToFrameIndex(int64_t pts) {
    double denom = static_cast<double>(nes::NTSC_FPS_DENOMINATOR);
    double numer = static_cast<double>(nes::NTSC_FPS_NUMERATOR);

    double fi = ((pts - m_Config->OffsetMillis) * numer) / (denom * 1000);
    return static_cast<int>(std::round(fi));
}

void VideoComponent::FindTargetFrame(int frameIndex) {
    UpdatePTSs();

    if (m_PTS.empty()) {
        SetBlankImage();
        return;
    }

    int64_t targetPts = FrameIndexToPTS(frameIndex);

    auto it = std::lower_bound(m_PTS.begin(), m_PTS.end(), targetPts);
    if (it == m_PTS.end()) {
        it--;
    }
    if (it != m_PTS.begin()) {
        int64_t a = *it;
        int64_t b = *std::prev(it);
        if (std::abs(targetPts - b) < std::abs(targetPts - a)) {
            it--;
        }
    }

    SetVideoFrame(std::distance(m_PTS.begin(), it));
}

void VideoComponent::SetImageFromInputFrame() {
    if (m_LiveInputFrame) {
        m_Image = cv::Mat(
                m_LiveInputFrame->Height,
                m_LiveInputFrame->Width,
                CV_8UC3,
                m_LiveInputFrame->Buffer);
        cv::resize(m_Image, m_Image, {}, m_Config->ScreenMultiplier, m_Config->ScreenMultiplier,
                cv::INTER_NEAREST);
    } else {
        SetBlankImage();
    }
}

void VideoComponent::SetVideoFrame(int videoIndex) {
    m_CurrentVideoIndex = videoIndex;
    m_LiveInputFrame = m_VideoThread->GetFrame(m_CurrentVideoIndex);
    if (m_LiveInputFrame->Height != nes::FRAME_HEIGHT || m_LiveInputFrame->Width != nes::FRAME_WIDTH) {
        throw std::runtime_error(fmt::format("Invalid video frame size '{}x{}'. Must be '{}x{}'",
            m_LiveInputFrame->Width, m_LiveInputFrame->Height, nes::FRAME_WIDTH, nes::FRAME_HEIGHT));

    }
    SetImageFromInputFrame();
}

void VideoComponent::SetBlankImage() {
    m_Image = cv::Mat::zeros(
            nes::FRAME_HEIGHT * m_Config->ScreenMultiplier,
            nes::FRAME_WIDTH * m_Config->ScreenMultiplier, CV_8UC3);
}

void VideoComponent::SetOffset(int offset) {
    m_Config->OffsetMillis = offset;
    m_EventQueue->PublishI(EventType::OFFSET_SET_TO, offset);
}

void VideoComponent::UpdateOffset(int dx) {
    int startFrame = m_CurrentVideoIndex;
    int endFrame = m_CurrentVideoIndex + dx;
    int n = static_cast<int>(m_PTS.size());
    if (startFrame >= 0 && startFrame < n &&
        endFrame >= 0 && endFrame < n) {

        int64_t oPts = m_PTS[startFrame];
        int64_t ePts = m_PTS[endFrame];

        int o = m_Config->OffsetMillis + (ePts - oPts);
        SetOffset(o);
        FindTargetFrame(m_InputTarget);
    }
}

std::string VideoComponent::WindowName() {
    return "Video";
}

void VideoComponent::OnFrame() {
    if (m_CurrentVideoIndex == -1) {
        FindTargetFrame(0);
    }
    if (ImGui::Begin(WindowName().c_str())) {
        rgmui::Mat("screen", m_Image);
        if (ImGui::IsItemHovered()) {
            auto& io = ImGui::GetIO();
            int m = static_cast<int>(-io.MouseWheel);

            if (m) {
                int v = m_CurrentVideoIndex + m;
                if (v < 0) {
                    v = 0;
                }
                if (v >= m_VideoThread->QueueSize()) {
                    v = m_VideoThread->QueueSize() - 1;
                }


                if (v != m_CurrentVideoIndex) {
                    SetVideoFrame(v);
                    int frameIndex = PTSToFrameIndex(m_PTS[v]);
                    m_EventQueue->PublishI(EventType::SET_INPUT_TARGET_TO, frameIndex);
                }
            }
        }

        if (m_CurrentVideoIndex >= 0 && m_CurrentVideoIndex < m_PTS.size()) {
            //ImGui::Text("%s : %ld [%ld]", m_VideoPath.c_str(), m_CurrentVideoIndex, m_PTS[m_CurrentVideoIndex]);

            ImGui::PushItemWidth(120);
            if (ImGui::InputInt("sync", &m_Config->OffsetMillis)) {
                SetOffset(m_Config->OffsetMillis);
                FindTargetFrame(m_InputTarget);
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("<")) {
                UpdateOffset(-1);
            }
            ImGui::SameLine();
            if (ImGui::Button(">")) {
                UpdateOffset(1);
            }
            ImGui::SameLine();
            if (ImGui::Button("reset here")) {
                SetOffset(m_PTS[m_CurrentVideoIndex]);
                m_EventQueue->PublishI(EventType::SET_INPUT_TARGET_TO, 0);
            }
        }
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////

PlaybackComponent::PlaybackComponent(rgmui::EventQueue* queue)
    : m_EventQueue(queue)
    , m_PlaybackSpeed(1.0f)
    , m_IsPlaying(false)
    , m_LastTime(util::Now())
    , m_Accumulator(util::mclock::duration(0))
{
}

PlaybackComponent::~PlaybackComponent() {
}

void PlaybackComponent::HandleHotkeys() {
    auto& io = ImGui::GetIO();
    int dx = 0;
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT)) {
            dx = -1;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT)) {
            dx =  1;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_BACKSPACE)) {
            dx = -1;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_BACKSLASH)) {
            dx =  1;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_UP)) {
            dx = -8;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_DOWN)) {
            dx =  8;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_PAGEUP)) {
            dx = -64;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_PAGEDOWN)) {
            dx =  64;
        }

        if (rgmui::ShiftIsDown()) {
            dx *= 4;
        }
        if (dx) {
            m_EventQueue->PublishI(EventType::SCROLL_INPUT_TARGET, dx);
        }

        if (ImGui::IsKeyPressed(SDL_SCANCODE_SPACE, false)) {
            TogglePlaying();
        }
    }
}

std::string PlaybackComponent::WindowName() {
    return "Playback";
}

void PlaybackComponent::TogglePlaying() {
    m_IsPlaying = !m_IsPlaying;
    if (!m_IsPlaying) {
        m_Accumulator = util::mclock::duration(0);
    }
}

void PlaybackComponent::HandlePlaying() {
    util::mclock::time_point v = util::Now();
    if (m_IsPlaying && (m_PlaybackSpeed != 0.0f)) {
        m_Accumulator += (v - m_LastTime);

        float speed = std::pow(std::abs(m_PlaybackSpeed), 1.8);

        int64_t frameDurationMillis = static_cast<int64_t>(
                std::round(
                    (static_cast<float>(nes::NTSC_FPS_DENOMINATOR) / static_cast<float>(nes::NTSC_FPS_NUMERATOR)) * 1000 / speed
                    )
                );
        if (m_PlaybackSpeed < 0) {
            frameDurationMillis *= -1;
        }

        if (frameDurationMillis != 0) {
            int64_t accumMillis = util::ToMillis(m_Accumulator);
            int d = accumMillis / frameDurationMillis;

            m_Accumulator -= util::ToDuration(frameDurationMillis * d);
            if (d != 0) {
                m_EventQueue->PublishI(EventType::SCROLL_INPUT_TARGET, d);
            }
        }


    }
    m_LastTime = v;
}

void PlaybackComponent::OnFrame() {
    HandleHotkeys();
    HandlePlaying();

    if (ImGui::Begin(WindowName().c_str())) {
        std::string txt = (m_IsPlaying) ? "Pause" : "Play";
        if (ImGui::Button(txt.c_str())) {
            TogglePlaying();
        }

        ImGui::SliderFloat("Speed", &m_PlaybackSpeed, -7.0f, 7.0f);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            m_PlaybackSpeed = 1.0f;
        }
    }
    ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////

static bool IsValidINESExtension(const std::string& extension) {
    return extension == ".nes" || extension == ".NES";
}
static bool IsValidVideoExtension(const std::string& extension) {
    return extension == ".mp4" || extension == ".mkv";
}

GraphiteConfigApp::GraphiteConfigApp(bool* wasExited, GraphiteConfig* config)
    : rgmui::IApplication(ThisApplicationConfig())
    , m_WasExited(wasExited)
    , m_Config(config)
{
    for (const auto & entry : fs::directory_iterator(".")) {
        if (IsValidINESExtension(entry.path().extension().string())) {
            m_PossibleInesPaths.push_back(entry.path().string());
        }
    }
    if (m_PossibleInesPaths.size() == 1) {
        m_Config->InesPath = m_PossibleInesPaths.front();
    }

    for (const auto & entry : fs::directory_iterator(".")) {
        if (IsValidVideoExtension(entry.path().extension().string())) {
            m_PossibleVideoPaths.push_back(entry.path().string());
        }
    }
    if (m_PossibleVideoPaths.size() == 1) {
        m_Config->VideoPath = m_PossibleVideoPaths.front();
        SetFM2PathFromVideoPath(m_Config);
    }
}

GraphiteConfigApp::~GraphiteConfigApp() {
}

rgmui::IApplicationConfig GraphiteConfigApp::ThisApplicationConfig() {
    rgmui::IApplicationConfig cfg = rgmui::IApplicationConfig::Defaults();
    cfg.CtrlWIsExit = false;
    cfg.DefaultDockspace = false;
    return cfg;
}

bool GraphiteConfigApp::OnSDLEvent(const SDL_Event& e) {
    if (rgmui::KeyDownWithCtrl(e, SDLK_w)) {
        *m_WasExited = true;
        return false;
    }

    return true;
}

bool GraphiteConfigApp::OnFrame() {
    bool ret = true;
    auto& io = ImGui::GetIO();

    if (m_Config->InesPath.empty()) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once,
                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Once);

        if (ImGui::Begin("Select Ines File")) {
            if (m_PossibleInesPaths.empty()) {
                ImGui::TextUnformatted("Please place the .nes file in the");
                ImGui::TextUnformatted("same directory as graphite.exe");
                if (ImGui::Button("ok")) {
                    *m_WasExited = true;
                    ret = false;
                }
            } else {
                for (auto & path : m_PossibleInesPaths) {
                    if (ImGui::Button(path.c_str())) {
                        m_Config->InesPath = path;
                    }
                }
            }
        }
        ImGui::End();
    } else if (m_Config->VideoPath.empty()) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once,
                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Once);

        if (ImGui::Begin("Select Video File")) {
            if (m_PossibleVideoPaths.empty()) {
                ImGui::TextUnformatted("Please place the .mp4 file in the");
                ImGui::TextUnformatted("same directory as graphite.exe");
                if (ImGui::Button("ok")) {
                    *m_WasExited = true;
                    ret = false;
                }
            } else {
                for (auto & path : m_PossibleVideoPaths) {
                    if (ImGui::Button(path.c_str())) {
                        m_Config->VideoPath = path;
                        SetFM2PathFromVideoPath(m_Config);
                    }
                }
            }
        }
        ImGui::End();
    } else {
        ret = false;
    }
    return ret;
}
