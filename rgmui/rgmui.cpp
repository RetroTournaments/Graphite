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
#define GL_BGR 0x80E0
#endif

#include "rgmui/rgmui.h"
#include "GL/gl.h"

#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

using namespace rgms::rgmui;

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

////////////////////////////////////////////////////////////////////////////////

Window::Window(int winsizex, int winsizey, const std::string& name) 
    : m_Window(nullptr)
    , m_Context(nullptr) 
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(SDL_GetError());
    }
    m_Window = SDL_CreateWindow(
        name.c_str(), 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED,
        winsizex, 
        winsizey, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    m_Context = SDL_GL_CreateContext(m_Window);
    if (m_Context == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    if (SDL_GL_MakeCurrent(m_Window, m_Context) != 0) {
        throw std::runtime_error(SDL_GetError());
    }
    
    //  -1 for adaptive 
    //   0 for immediate 
    //   1 for synchronized
    int swpInterval = 0;
    SDL_GL_SetSwapInterval(swpInterval);

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL2_InitForOpenGL(m_Window, m_Context)) {
        throw std::runtime_error("Failure initializing sdl for opengl, maybe update your graphics driver?");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        throw std::runtime_error("Failure initializing opengl, maybe update your graphics driver?");
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigWindowsMoveFromTitleBarOnly = true;
}

Window::~Window() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (m_Context) SDL_GL_DeleteContext(m_Context);
    if (m_Window) SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

bool Window::OnSDLEvent(const SDL_Event& e) {
    ImGui_ImplSDL2_ProcessEvent(&e);
    if (e.type == SDL_QUIT) {
        return false;
    }
    return true;
}

int Window::ScreenWidth() const {
    int w;
    SDL_GetWindowSize(m_Window, &w, NULL);
    return w;
}

int Window::ScreenHeight() const {
    int h;
    SDL_GetWindowSize(m_Window, NULL, &h);
    return h;
}

void Window::NewFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(m_Window);
    ImGui::NewFrame();

    glViewport(0, 0, ScreenWidth(), ScreenHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Window::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(m_Window);
}

////////////////////////////////////////////////////////////////////////////////
//
IApplicationComponent::IApplicationComponent() {
}

IApplicationComponent::~IApplicationComponent() {
}

void IApplicationComponent::OnSDLEvent(const SDL_Event& e) {
    OnSubComponentEvents(e);
}

void IApplicationComponent::OnFrame() {
    OnSubComponentFrames();
}

void IApplicationComponent::RegisterSubComponent(IApplicationComponent* component) {
    m_SubComponents.push_back(component);
}

void IApplicationComponent::RegisterSubComponent(std::shared_ptr<IApplicationComponent> component) {
    m_OwnedComponents.push_back(component);
    m_SubComponents.push_back(component.get());
}

void IApplicationComponent::OnSubComponentEvents(const SDL_Event& e) {
    for (auto & comp : m_SubComponents) {
        comp->OnSDLEvent(e);
    }
}

void IApplicationComponent::OnSubComponentFrames() {
    for (auto & comp : m_SubComponents) {
        comp->OnFrame();
    }
}

////////////////////////////////////////////////////////////////////////////////

IApplicationConfig IApplicationConfig::Defaults() {
    IApplicationConfig cfg;
    cfg.CtrlWIsExit = true;
    cfg.DefaultDockspace = true;
    return cfg;
}

IApplication::IApplication(IApplicationConfig config) 
    : m_Config(config)
    , m_DockspaceID(0)
{
}

IApplication::~IApplication() {
}

bool IApplication::OnSDLEventExternal(const SDL_Event& e) {
    if (m_Config.CtrlWIsExit && KeyDownWithCtrl(e, SDLK_w)) {
        return false;
    }

    bool r = OnSDLEvent(e);
    if (r) {
        for (auto & comp : m_Components) {
            comp->OnSDLEvent(e);
        }
    }
    return r;
}

ImGuiID IApplication::GetDefaultDockspaceID() {
    return m_DockspaceID;
}

bool IApplication::OnFrameExternal() {
    if (m_Config.DefaultDockspace) {
        m_DockspaceID = ImGui::DockSpaceOverViewport();
    }
    bool r = OnFrame();

    if (r) {
        for (auto & comp : m_Components) {
            comp->OnFrame();
        }
    }
    return r;
}

bool IApplication::OnSDLEvent(const SDL_Event& e) {
    return true;
}

bool IApplication::OnFrame() {
    return true;
}

void IApplication::RegisterComponent(IApplicationComponent* component) {
    m_Components.push_back(component);
}

void IApplication::RegisterComponent(std::shared_ptr<IApplicationComponent> component) {
    m_OwnedComponents.push_back(component);
    m_Components.push_back(component.get());
}

////////////////////////////////////////////////////////////////////////////////

bool rgms::rgmui::KeyUp(const SDL_Event& e, SDL_Keycode k) {
    return e.type == SDL_KEYUP && e.key.keysym.sym == k;
}

bool rgms::rgmui::KeyUpWithCtrl(const SDL_Event& e, SDL_Keycode k) {
    if (KeyUp(e, k)) {
        const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
        if (keystates[SDL_SCANCODE_LCTRL] || keystates[SDL_SCANCODE_RCTRL]) {
            return true;
        }
    }
    return false;
}

bool rgms::rgmui::KeyDown(const SDL_Event& e, SDL_Keycode k) {
    return e.type == SDL_KEYDOWN && e.key.keysym.sym == k;
}

bool rgms::rgmui::KeyDownWithCtrl(const SDL_Event& e, SDL_Keycode k) {
    if (KeyDown(e, k)) {
        const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
        if (keystates[SDL_SCANCODE_LCTRL] || keystates[SDL_SCANCODE_RCTRL]) {
            return true;
        }
    }
    return false;
}

bool rgms::rgmui::ShiftIsDown() {
    const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
    return keystates[SDL_SCANCODE_LSHIFT] || keystates[SDL_SCANCODE_RSHIFT];
}

bool rgms::rgmui::ArrowKeyHelper(const SDL_Event& e, std::function<void(int dx, int dy)> cback,
        int shiftMultiplier) {
    if (e.type == SDL_KEYDOWN) {
        int dx = 0;
        int dy = 0;
        switch (e.key.keysym.sym) {
            case SDLK_UP:
                dx = 0;
                dy = -1;
                break;
            case SDLK_DOWN:
                dx = 0;
                dy = 1;
                break;
            case SDLK_LEFT:
                dx = -1;
                dy = 0;
                break;
            case SDLK_RIGHT:
                dx = 1;
                dy = 0;
                break;
            default:
                break;
        }

        if (dx || dy) {
            if (ShiftIsDown()) {
                dx *= shiftMultiplier;
                dy *= shiftMultiplier;
            }

            cback(dx, dy);
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

EventQueue::EventQueue() {
}

EventQueue::~EventQueue() {
}

void EventQueue::Publish(int etype) {
    Event e;
    e.EventType = etype;
    e.Data = nullptr;
    Publish(e);
}

void EventQueue::Publish(int etype, std::shared_ptr<void> data) {
    Event e;
    e.EventType = etype;
    e.Data = data;
    Publish(e);
}

void EventQueue::PublishI(int etype, int dataInt) {
    Event e;
    e.EventType = etype;
    e.Data = std::make_shared<int>(dataInt);
    Publish(e);
}

void EventQueue::PublishS(int etype, std::string dataStr) {
    Event e;
    e.EventType = etype;
    e.Data = std::make_shared<std::string>(dataStr);
    Publish(e);
}

void EventQueue::Publish(Event e) {
    m_PendingEvents.push_back(e);
}

void EventQueue::Subscribe(int etype, EventCallback cback) {
    m_Callbacks[etype].push_back(cback);
}

void EventQueue::Subscribe(int etype, std::function<void()> cback) {
    m_Callbacks[etype].push_back([cback](const Event&){
        cback();
    });
}

void EventQueue::SubscribeI(int etype, std::function<void(int v)> cback) {
    Subscribe(etype, [cback](const Event& e){
        cback(*reinterpret_cast<int*>(e.Data.get()));
    });
}

void EventQueue::SubscribeS(int etype, std::function<void(const std::string& v)> cback) {
    Subscribe(etype, [cback](const Event& e){
        cback(*reinterpret_cast<std::string*>(e.Data.get()));
    });
}

void EventQueue::PumpQueue() {
    if (!m_PendingEvents.empty()) {
        std::vector<Event> events;
        std::swap(m_PendingEvents, events);

        for (auto & e : events) {
            auto it = m_Callbacks.find(e.EventType);
            if (it != m_Callbacks.end()) {
                for (auto & v : it->second) {
                    v(e);
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool rgms::rgmui::IsAnyPopupOpen() {
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId + ImGuiPopupFlags_AnyPopupLevel);
}

// See misc/cpp/imgui_stdlib
static int InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = reinterpret_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = const_cast<char*>(str->c_str());
    }
    return 0;
}

bool rgms::rgmui::InputText(const char* label, std::string* str, 
        ImGuiInputTextFlags flags) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    bool ret = ImGui::InputText(label, const_cast<char*>(str->c_str()), 
        str->capacity() + 1, flags, InputTextCallback, (void*)str);
    ImGui::PushID("context");
    if (ImGui::BeginPopupContextItem(label, ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::Selectable("copy")) {
            ImGui::SetClipboardText(str->c_str());
        }
        if (ImGui::Selectable("paste")) {
            *str = ImGui::GetClipboardText();
        }
        if (ImGui::Selectable("clear")) {
            *str = "";
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    return ret;
}

template <typename T>
static bool SliderExt(T* v, T min, T max, bool allowArrowKeys, bool allowMouseWheel, T singleMove, T shiftMult) {
    bool changed = false;

    if (ImGui::IsItemHovered()) {
        T m = 0;
        if (allowArrowKeys && 
                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT)) {
                m = -1;
            } else if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT)) {
                m = 1;
            } 
        }

        if (m == 0 && allowMouseWheel) {
            auto& io = ImGui::GetIO();
            m = static_cast<T>(-io.MouseWheel);
        }

        m *= singleMove;

        if (m && ShiftIsDown()) {
            m *= shiftMult;
        }

        if (m) {
            T nv = *v + static_cast<T>(m);
            if (nv < min) {
                nv = min;
            }
            if (nv > max) {
                nv = max;
            }

            if (nv != *v) {
                *v = nv;
                changed = true;
            }
        }
    }
    return changed;
}

bool rgms::rgmui::SliderIntExt(const char* label, int* v, int min, int max,
        const char* format, ImGuiSliderFlags flags,
        bool allowArrowKeys, bool allowMouseWheel,
        int singleMove, int shiftMult) {
    bool changed = ImGui::SliderInt(label, v, min, max, format, flags);
    changed = SliderExt(v, min, max, allowArrowKeys, allowMouseWheel, singleMove, shiftMult) || changed;
    return changed;
}

bool rgms::rgmui::SliderFloatExt(const char* label, float* v, float min, float max,
        const char* format, ImGuiSliderFlags flags,
        bool allowArrowKeys, bool allowMouseWheel,
        float singleMove, float shiftMult) {
    bool changed = ImGui::SliderFloat(label, v, min, max, format, flags);
    changed = SliderExt(v, min, max, allowArrowKeys, allowMouseWheel, singleMove, shiftMult) || changed;
    return changed;
}


void rgms::rgmui::Mat(const char* label, const cv::Mat& img) {
    if (!img.isContinuous()) {
        cv::Mat m = img.clone();
        assert(m.isContinuous());
        Mat(label, m);
    }
    static std::unordered_map<ImGuiID, GLuint> s_GLuints;
    ImGuiID v = ImGui::GetID(label);

    auto it = s_GLuints.find(v);
    if (it != s_GLuints.end()) {
        glDeleteTextures(1, &it->second);
    }

    GLuint q = 0;
    glGenTextures(1, &q);
    s_GLuints[v] = q;
    glBindTexture(GL_TEXTURE_2D, q);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0,
            GL_BGR, GL_UNSIGNED_BYTE, img.data);

    ImGui::Image((void*)(intptr_t)(q), ImVec2(static_cast<float>(img.cols), static_cast<float>(img.rows)));
}


////////////////////////////////////////////////////////////////////////////////
cv::Mat rgms::rgmui::CropWithZeroPadding(cv::Mat img, cv::Rect cropRect) {
    if (img.empty()) {
        return img;
    }

    cv::Rect imgRect = cv::Rect(0, 0, img.cols, img.rows);
    cv::Rect overLapRect = imgRect & cropRect;
    if (overLapRect.width == cropRect.width && overLapRect.height == cropRect.height) {
        return img(overLapRect);
    }

    // add padding still
    cv::Mat m = cv::Mat::zeros(cropRect.height, cropRect.width, img.type());
    if (overLapRect.width > 0 && overLapRect.height > 0) {
        img(overLapRect).copyTo(m(cv::Rect(overLapRect.x - cropRect.x, overLapRect.y - cropRect.y, overLapRect.width, overLapRect.height)));
    }
    return m;
}

cv::Mat rgms::rgmui::ConstructPaletteImage(
    const uint8_t* imgData,
    int width, int height, 
    const uint8_t* paletteData,
    PaletteDataOrder paletteDataOrder
) {

    cv::Mat m(height, width, CV_8UC3);

    uint8_t* o = reinterpret_cast<uint8_t*>(m.data);

    int a = 0;
    int b = 1;
    int c = 2;

    if (paletteDataOrder == PaletteDataOrder::RGB) {
        std::swap(a, c);
    }

    for (int k = 0; k < (width * height); k++) {
        const uint8_t* p = paletteData + (*imgData * 3);

        *(o + 0) = *(p + a);
        *(o + 1) = *(p + b);
        *(o + 2) = *(p + c);

        o += 3;
        imgData++;
    }
    return m;

}
