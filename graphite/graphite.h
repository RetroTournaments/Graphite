////////////////////////////////////////////////////////////////////////////////
//
// The Graphite Library contains the application, and the components which make
// up that application.
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

#ifndef GRAPHITE_GRAPHITE_HEADER
#define GRAPHITE_GRAPHITE_HEADER

#include <string>
#include <unordered_set>

#include "graphite/rgmui.h"
#include "graphite/nes.h"
#include "graphite/video.h"

namespace graphite {

struct InputChangeEvent {
    InputChangeEvent(int frameIndex, nes::ControllerState newState);

    int FrameIndex;
    nes::ControllerState NewState;
};

enum EventType : int {
    INPUT_TARGET_SET_TO,  // int
    SET_INPUT_TARGET_TO,  // int
    SCROLL_INPUT_TARGET,  // int (delta)

    NES_FRAME_SET_TO, // int
    NES_STATE_SET_TO, // std::string

    INPUT_SET_TO, // InputChangeEvent
    OFFSET_SET_TO, // int
    SET_OFFSET_TO, // int

    REQUEST_SAVE,
    REFRESH_CONFIG,
};

struct EmuViewConfig;
class NESEmulatorComponent : public rgmui::IApplicationComponent {
public:
    NESEmulatorComponent(rgmui::EventQueue* queue, 
            const std::string& inesPath,
            EmuViewConfig* emuViewConfig);
    ~NESEmulatorComponent();

    virtual void OnFrame() override;

private:
    static nes::NESEmulatorFactorySPtr InitializeEmulatorFactory(const std::string& inesPath);

private:
    rgmui::EventQueue* m_EventQueue;

    nes::NESEmulatorFactorySPtr m_EmulatorFactory;
    nes::StateSequenceThread m_StateSequenceThread;
};

struct InputsConfig {
    int ColumnPadding;
    int ChevronColumnWidth;
    int FrameTextColumnWidth;
    int ButtonWidth;
    int FrameTextNumDigits;
    int MaxInputSize;
    int ScrollOffset;

    bool StickyAutoScroll;

    ImU32 TextColor;
    ImU32 HighlightTextColor;
    ImU32 ButtonColor;

    static InputsConfig Defaults();
};

class UndoRedo {
public:
    UndoRedo(rgmui::EventQueue* queue,
             std::vector<nes::ControllerState>* inputs);
    ~UndoRedo();

    // Undo / redoable action
    void ChangeInputTo(int frameIndex, nes::ControllerState newState);

    void Undo();
    void Redo();

    void ConsolidateLast(int count);

private:
    void IntChangeInput(int frameIndex, nes::ControllerState newState);

private:
    rgmui::EventQueue* m_EventQueue;
    std::vector<nes::ControllerState>* m_Inputs;

    int m_ChangeIndex;
    std::vector<size_t> m_ChangeIndices;
    struct Change {
        int FrameIndex;
        nes::ControllerState OldState;
        nes::ControllerState NewState;
        bool Consolidated;

        Change(int _frameIndex, nes::ControllerState _oldState, nes::ControllerState _newState);
        Change();
    };
    std::vector<Change> m_Changes;
};

inline const std::string FM2_OFFSET_COMMENT_LINE_START = "comment offset ";
class InputsComponent : public rgmui::IApplicationComponent {
public:
    InputsComponent(
            rgmui::EventQueue* queue,
            const std::string& fm2Path,
            InputsConfig* config
            );
    ~InputsComponent();

    virtual void OnFrame() override;
    virtual void OnSDLEvent(const SDL_Event& e) override;

    static std::string WindowName();

private:
    class TargetScroller {
    public:
        TargetScroller(InputsComponent* inputs);
        ~TargetScroller();

        void DoButtons();
        void UpdateScroll();

        void SuspendAutoScroll();
        void OnTargetChange(bool byChevronColumn);

    private:
        void SetScrollDirectTo(int target);
        float TargetY(int target);

    private:
        InputsComponent* m_InputsComponent;
        bool m_AutoScroll;
        bool m_AutoScrollWasSetOnByUser;
        bool m_ScrolledNext;
        int m_LastScrollTarget;
        float m_LastScrollY;
        float m_TargetScrollY;
        float m_CurrentMaxY;
        float m_VisibleY;
    };

private:
    std::string FrameText(int frameId) const;
    ImVec2 CalcLineSize();
    void DoInputLine(int frameIndex);
    void DrawButton(ImDrawList* list, ImVec2 ul, ImVec2 lr, uint8_t button, bool buttonOn, bool highlighted);
    void DoInputList(ImVec2 screenPos, int startIndex, int endIndex);
    void DoMainMenuBar();

    void ChangeInputTo(int frameIndex, nes::ControllerState newInput);
    void ChangeButtonTo(int frameIndex, uint8_t button, bool onoff);
    void ChangeTargetTo(int frameIndex, bool byChevronColumn);
    ImU32 TextColor(bool highlighted);
    std::string ButtonText(uint8_t button);


private:
    std::string OffsetLine() const;
    void TryReadFM2();
    void WriteFM2();

    nes::FM2Header m_Header;
    std::string m_FM2Path;
    int m_OffsetMillis; 


private:
    rgmui::EventQueue* m_EventQueue;
    TargetScroller m_TargetScroller;

    ImVec2 m_LineSize;
    std::vector<int> m_ColumnX;

    bool m_AllowDragging;
    bool m_TargetDragging;
    bool m_CouldToggleLock;
    int m_LockTarget;

    struct DragInfo {
        bool IsDragging;
        int StartFrameIndex;
        uint8_t Button; // 0x00 on frame
        uint8_t HLButtons;
        bool StartedOn;
        std::unordered_set<int> HighlightedFrames;
        int LastHighlighted;

        void StartDrag(int frameIndex, uint8_t button, uint8_t hlButtons, bool on);
        void Clear();
    } m_Drag;

    InputsConfig* m_Config;
    UndoRedo m_UndoRedo;

    std::vector<nes::ControllerState> m_Inputs;
    int m_TargetIndex;
    int m_CurrentIndex;
};

class IEmuPeekSubComponent : public rgmui::IApplicationComponent {
public:
    IEmuPeekSubComponent();
    virtual ~IEmuPeekSubComponent();

    virtual void CacheNewEmulatorData(nes::INESEmulator* emu) = 0;
    virtual void OnFrame() = 0;
};

struct ScreenPeekConfig {
    int ScreenMultiplier;
    nes::Palette NESPalette;

    static ScreenPeekConfig Defaults();
};

class ScreenPeekSubComponent : public IEmuPeekSubComponent {
public:
    ScreenPeekSubComponent(rgmui::EventQueue* queue, ScreenPeekConfig* config);
    virtual ~ScreenPeekSubComponent();

    virtual void CacheNewEmulatorData(nes::INESEmulator* emu) override;
    virtual void OnFrame() override;

    static std::string WindowName();

private:
    void SetBlankImage();
    void ConstructImageFromFrame();
    
    std::string PositionText(int x, int y) const;
    std::string PixelText(uint8_t pixel) const;
    ImVec4 PeekPixelColor(uint8_t pixel) const;
    bool GetScreenPeekPixel(const ImVec2& cursorPos, const ImVec2& mousePos,
            int* x, int* y, uint8_t* pixel) const;
    void DoPopupItems(int x, int y, uint8_t pixel);
    void DoHoverTooltip(int x, int y, uint8_t pixel);

private:
    rgmui::EventQueue* m_EventQueue;
    ScreenPeekConfig* m_Config;

    nes::Frame m_Frame;
    cv::Mat m_Image;
};

struct RAMWatchLine {
    RAMWatchLine(bool isSep, std::string name, uint16_t addr);

    bool IsSeparator;
    std::string Name;
    uint16_t Address;
};


struct RAMWatchConfig {
    std::vector<RAMWatchLine> Lines;

    static RAMWatchConfig Defaults();
    static RAMWatchConfig SMBDefaults();
};

class RAMWatchSubComponent : public IEmuPeekSubComponent {
public: 
    RAMWatchSubComponent(rgmui::EventQueue* queue, RAMWatchConfig* config);
    virtual ~RAMWatchSubComponent();

    virtual void CacheNewEmulatorData(nes::INESEmulator* emu) override;
    virtual void OnFrame() override;

    static std::string WindowName();

private:
    rgmui::EventQueue* m_EventQueue;
    RAMWatchConfig* m_Config;
    nes::Ram m_RAM;
};



struct EmuViewConfig {
    ScreenPeekConfig ScreenPeekCfg;
    RAMWatchConfig RAMWatchCfg;
    nes::StateSequenceThreadConfig StateSequenceThreadCfg;

    static EmuViewConfig Defaults();
};

class EmuViewComponent : public rgmui::IApplicationComponent {
public:
    EmuViewComponent(rgmui::EventQueue* queue,
            std::unique_ptr<nes::INESEmulator>&& emu,
            EmuViewConfig* config);
    ~EmuViewComponent();

    virtual void OnFrame() override;

private:
    void RegisterEmuPeekComponent(std::shared_ptr<IEmuPeekSubComponent> comp);

private:
    rgmui::EventQueue* m_EventQueue;
    std::unique_ptr<nes::INESEmulator> m_Emulator;

    std::vector<std::shared_ptr<IEmuPeekSubComponent>> m_EmuComponents;
};

struct VideoConfig {
    int MaxFrames;
    int ScreenMultiplier;
    int OffsetMillis;

    static VideoConfig Defaults();
};

class VideoComponent : public rgmui::IApplicationComponent {
public:
    VideoComponent(rgmui::EventQueue* queue,
            const std::string& videoPath,
            VideoConfig* videoCfg);
    ~VideoComponent();

    virtual void OnFrame() override;

    static std::string WindowName();

private:
    int64_t FrameIndexToPTS(int frameIndex);
    int PTSToFrameIndex(int64_t pts);

    void SetBlankImage();
    void FindTargetFrame(int frameIndex);
    void UpdatePTSs();
    void SetOffset(int offset);
    void UpdateOffset(int dx);

    void SetVideoFrame(int videoIndex);
    void SetImageFromInputFrame();

private:
    rgmui::EventQueue* m_EventQueue;
    VideoConfig* m_Config;
    std::string m_VideoPath;
    std::unique_ptr<video::LiveInputThread> m_VideoThread;

    int64_t m_CurrentVideoIndex;
    int m_InputTarget;

    std::vector<int64_t> m_PTS;
    video::LiveInputFramePtr m_LiveInputFrame;
    cv::Mat m_Image;
};

class PlaybackComponent : public rgmui::IApplicationComponent {
public:
    PlaybackComponent(rgmui::EventQueue* queue);
    ~PlaybackComponent();

    virtual void OnFrame() override;

    static std::string WindowName();

private:
    void HandleHotkeys();
    void HandlePlaying();
    void TogglePlaying();

private:
    rgmui::EventQueue* m_EventQueue;
    float m_PlaybackSpeed;
    bool m_IsPlaying;
    util::mclock::time_point m_LastTime;
    util::mclock::duration m_Accumulator;
};

////////////////////////////////////////////////////////////////////////////////
// Graphite Config
////////////////////////////////////////////////////////////////////////////////
struct GraphiteConfig {
    std::string InesPath;
    std::string VideoPath;
    std::string FM2Path;

    bool DoInitialDockspaceSetup;

    InputsConfig InputsCfg;
    EmuViewConfig EmuViewCfg;
    VideoConfig VideoCfg;

    static GraphiteConfig Defaults();
};
bool ParseArgumentsToConfig(int* argc, char*** argv, GraphiteConfig* config);
void SetFM2PathFromVideoPath(GraphiteConfig* config);
class GraphiteConfigApp : public rgmui::IApplication {
public:
    GraphiteConfigApp(bool* wasExited, GraphiteConfig* config);
    ~GraphiteConfigApp();

    virtual bool OnSDLEvent(const SDL_Event& e) override;
    virtual bool OnFrame() override;

private:
    static rgmui::IApplicationConfig ThisApplicationConfig();
    bool* m_WasExited;
    GraphiteConfig* m_Config;

    std::vector<std::string> m_PossibleInesPaths;
    std::vector<std::string> m_PossibleVideoPaths;
};

////////////////////////////////////////////////////////////////////////////////
// Graphite App
////////////////////////////////////////////////////////////////////////////////
class GraphiteApp : public rgmui::IApplication {
public:
    GraphiteApp(GraphiteConfig* config);
    ~GraphiteApp();

    virtual bool OnFrame() override;

private:
    void SetupDockSpace();
    bool DoMainMenuBar();

private:
    bool m_FirstFrame;
    GraphiteConfig* m_Config;
    rgmui::EventQueue m_EventQueue;
};

}

#endif
