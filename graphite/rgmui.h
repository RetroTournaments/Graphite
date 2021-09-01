////////////////////////////////////////////////////////////////////////////////
//
// The RGMUI Library contains the building blocks of this particular style of
// ImGui based applications
//
// Essentially: Define an IApplication, a series of IApplicationComponents and
// then rig them all together by registering them to one another. Inter
// component communication is typically done through an EventQueue.
//
// This is a bit different than more conventional callback based applications,
// and perhaps a bit stranger. But it works?
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

#ifndef GRAPHITE_RGMUI_HEADER
#define GRAPHITE_RGMUI_HEADER

#include <thread>
#include <vector>
#include <memory>
#include <functional>

#include "SDL.h" 
#include "imgui.h"
#include "opencv2/opencv.hpp"

#include "graphite/util.h"


namespace graphite::rgmui {

class Window;
class IApplication;

void WindowAppMainLoop(
    Window* window, IApplication* application,
    util::mclock::duration minimumFrameDuration = util::mclock::duration(0));

////////////////////////////////////////////////////////////////////////////////

class Window {
public:
    Window(int winsizex, int winsizey, const std::string& name);
    ~Window();

    int ScreenWidth() const;
    int ScreenHeight() const;

    bool OnSDLEvent(const SDL_Event& e);

    void NewFrame();
    void EndFrame();

private:
    SDL_Window* m_Window;
    SDL_GLContext m_Context;
};

////////////////////////////////////////////////////////////////////////////////

class IApplicationComponent;

struct IApplicationConfig {
    bool CtrlWIsExit;
    bool DefaultDockspace;

    static IApplicationConfig Defaults();
};

class IApplication {
public:
    IApplication(IApplicationConfig config = IApplicationConfig::Defaults());
    virtual ~IApplication();
    virtual bool OnSDLEventExternal(const SDL_Event& e) final;
    virtual bool OnFrameExternal() final;

protected:
    // Return true on continue running the application
    // Override these if necessary
    virtual bool OnSDLEvent(const SDL_Event& e); // Default call subcomponent
    virtual bool OnFrame();

    void RegisterComponent(IApplicationComponent* component);
    void RegisterComponent(std::shared_ptr<IApplicationComponent> component);

private:
    IApplicationConfig m_Config;
    std::vector<IApplicationComponent*> m_Components;
    std::vector<std::shared_ptr<IApplicationComponent>> m_OwnedComponents;
};

class IApplicationComponent {
public:
    IApplicationComponent();
    virtual ~IApplicationComponent();

    virtual void OnSDLEvent(const SDL_Event& e); // default calls sub component OnEvents
    virtual void OnFrame(); // default calls sub component OnFrames

protected:
    void RegisterSubComponent(IApplicationComponent* component);
    void RegisterSubComponent(std::shared_ptr<IApplicationComponent> component);

    void OnSubComponentEvents(const SDL_Event& e);
    void OnSubComponentFrames();
    std::vector<IApplicationComponent*> m_SubComponents;
    std::vector<std::shared_ptr<IApplicationComponent>> m_OwnedComponents;
};

////////////////////////////////////////////////////////////////////////////////
// Event queue
////////////////////////////////////////////////////////////////////////////////
struct Event {
    int EventType;  // Generally define your own enum
    std::shared_ptr<void> Data;
};
typedef std::function<void(const Event& e)> EventCallback;

class EventQueue {
public:
    EventQueue();
    ~EventQueue();

    void Publish(int etype);
    void Publish(int etype, std::shared_ptr<void> data);
    void PublishI(int etype, int dataInt);
    void PublishS(int etype, std::string dataStr);
    void Publish(Event e);
    void Subscribe(int etype, EventCallback cback);
    void SubscribeI(int etype, std::function<void(int v)> cback);
    void SubscribeS(int etype, std::function<void(const std::string& v)> cback);

    size_t PendingEventsSize() const;
    void PumpQueue();

private:
    std::vector<Event> m_PendingEvents;
    std::unordered_map<int, std::vector<EventCallback>> m_Callbacks;
};




////////////////////////////////////////////////////////////////////////////////
// SDL Extensions
////////////////////////////////////////////////////////////////////////////////
bool KeyUp(const SDL_Event& e, SDL_Keycode k);
bool KeyUpWithCtrl(const SDL_Event& e, SDL_Keycode k);
bool KeyDown(const SDL_Event& e, SDL_Keycode k);
bool KeyDownWithCtrl(const SDL_Event& e, SDL_Keycode k);

bool ShiftIsDown();

// Convert arrow key movements into +/- dx, +/- dy
// optionally multiply when shift is down
bool ArrowKeyHelper(const SDL_Event& e,
        std::function<void(int dx, int dy)> cback,
        int shiftMultiplier = 1);


////////////////////////////////////////////////////////////////////////////////
// ImGui Extensions
////////////////////////////////////////////////////////////////////////////////

// Favorite is : ImGuiInputTextFlags_EnterReturnsTrue
bool InputText(const char* label, std::string* str, 
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

// When hovered these sliders adjust the values based on arrow keys / mouse
// wheels
bool SliderIntExt(const char* label, int* v, int min, int max, 
        const char* format = "%d", ImGuiSliderFlags flags = 0,
        bool allowArrowKeys = true, bool allowMouseWheel = true,
        int singleMove = 1, int shiftMult = 8);
bool SliderFloatExt(const char* label, float* v, float min, float max,
        const char* format = "%.3f", ImGuiSliderFlags flags = 0,
        bool allowArrowKeys = true, bool allowMouseWheel = true,
        float singleMove = 0.10f, float shiftMult = 4.0f);

// Display a cv mat
void Mat(const char* label, const cv::Mat& img);

bool IsAnyPopupOpen();

// easier combo boxes
template <typename T, typename Q>
bool Combo2(const char* label, T* v, const Q& container, 
        std::function<std::string(const T&)> toStr) {
    bool changed = false;

    if (ImGui::BeginCombo(label, toStr(*v).c_str())) {
        for (auto & x : container) {
            bool selected = *v == x;
            if (ImGui::Selectable(toStr(x).c_str(), selected)) {
                *v = x;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename T, typename Q>
bool Combo(const char* label, T* v, const Q& container) {
    auto toStr = [&](const T& t) {
        std::ostringstream os;
        os << t;
        return os.str();
    };
    return Combo2<T, Q>(label, v, container, toStr);
}

////////////////////////////////////////////////////////////////////////////////
// OpenCV Extensions
////////////////////////////////////////////////////////////////////////////////
cv::Mat CropWithZeroPadding(cv::Mat img, cv::Rect r);

// 
enum class PaletteDataOrder {
    RGB,
    BGR
};
cv::Mat ConstructPaletteImage(
    const uint8_t* imgData,
    int width, int height, 
    const uint8_t* paletteData,
    PaletteDataOrder paletteDataOrder = PaletteDataOrder::RGB
);

}

#endif

