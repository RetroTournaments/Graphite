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

#include <fstream>

#include "fmt/core.h"

#include "carbon/carbon.h"

using namespace carbon;
using namespace rgms;

FilterConfig FilterConfig::Defaults() {
    FilterConfig cfg;
    float w = 256;
    float h = 240;
    cfg.OutWidth = static_cast<int>(w);
    cfg.OutHeight = static_cast<int>(h);
    cfg.Type = FilterType::CROP;
    cfg.Crop = util::Rect2F(0, 0, w, h);
    cfg.Quad[0] = {0.0f, 0.0f};
    cfg.Quad[1] = {w, 0.0f};
    cfg.Quad[2] = {w, h};
    cfg.Quad[3] = {0.0f, h};
    cfg.Patch = util::RectanglePatch({0, 0}, {w, h});
    return cfg;
}

cv::Mat carbon::GetFilterPerspectiveMatrix(int width, int height, const FilterConfig& filter) {
    cv::Point2f inquad[4];
    for (int i = 0; i < 4; i++) {
        inquad[i] = cv::Point2f(filter.Quad[i].x, filter.Quad[i].y);
    }

    cv::Point2f outquad[4];
    outquad[0] = cv::Point2f(0, 0);
    outquad[1] = cv::Point2f(width, 0);
    outquad[2] = cv::Point2f(width, height);
    outquad[3] = cv::Point2f(0, height);

    return cv::getPerspectiveTransform(inquad, outquad);
}

cv::Mat carbon::ApplyFilter(const cv::Mat img, const FilterConfig& filter, 
        const rgms::video::PixelContributions& contributions) {
    if (filter.Type == FilterType::CROP) {
        cv::Mat ret = rgms::rgmui::CropWithZeroPadding(img, 
                cv::Rect(filter.Crop.X, filter.Crop.Y, filter.Crop.Width, filter.Crop.Height));
        if (!(ret.rows == 0 || ret.cols == 0)) {
            cv::resize(ret, ret, {filter.OutWidth, filter.OutHeight}, 0, 0, cv::INTER_AREA);
        }
        return ret;
    } else if (filter.Type == FilterType::UNCRT) {
        return rgms::video::RemoveCRT(img, contributions, filter.OutWidth, filter.OutHeight);
    } else if (filter.Type == FilterType::PERSPECTIVE) {
        cv::Mat ret;
        cv::Mat m = GetFilterPerspectiveMatrix(img.cols, img.rows, filter);
        cv::warpPerspective(img, ret, m, img.size());

        ret = rgms::rgmui::CropWithZeroPadding(ret, 
                cv::Rect(filter.Crop.X, filter.Crop.Y, filter.Crop.Width, filter.Crop.Height));
        if (!(ret.rows == 0 || ret.cols == 0)) {
            cv::resize(ret, ret, {filter.OutWidth, filter.OutHeight}, 0, 0, cv::INTER_AREA);
        }
        return ret;
    }
    if (img.cols != filter.OutWidth || img.rows != filter.OutHeight) {
        cv::Mat ret;
        cv::resize(img, ret, {filter.OutWidth, filter.OutHeight}, 0, 0, cv::INTER_AREA);
        return ret;
    }
    return img;
}

void FilterConfig::FromString(const std::string& str) {
    std::istringstream is(str);
    std::string line;
    std::getline(is, line, '_');
    if (line == "none") {
        Type = FilterType::NONE;
    } else if (line == "crop") {
        Type = FilterType::CROP;
    } else if (line == "perspective") {
        Type = FilterType::PERSPECTIVE;
    } else if (line == "uncrt") {
        Type = FilterType::UNCRT;
    } else {
        throw std::invalid_argument("not a valid filter string");
    }

    std::getline(is, line, '_');
    OutWidth = std::stoi(line);
    std::getline(is, line, '_');
    OutHeight = std::stoi(line);

    std::getline(is, line, '_');
    Crop.X = std::stof(line);
    std::getline(is, line, '_');
    Crop.Y = std::stof(line);
    std::getline(is, line, '_');
    Crop.Width = std::stof(line);
    std::getline(is, line, '_');
    Crop.Height = std::stof(line);

    for (int i = 0; i < 4; i++) {
        std::getline(is, line, '_');
        Quad[i].x = std::stof(line);
        std::getline(is, line, '_');
        Quad[i].y = std::stof(line);
    }

    for (int i = 0; i < 16; i++) {
        std::getline(is, line, '_');
        Patch[i].x = std::stof(line);
        std::getline(is, line, '_');
        Patch[i].y = std::stof(line);
    }
}

std::string FilterConfig::ToString() const {
    std::ostringstream os;
    if (Type == FilterType::NONE) {
        os << "none_";
    } else if (Type == FilterType::CROP) {
        os << "crop_";
    } else if (Type == FilterType::PERSPECTIVE) {
        os << "perspective_";
    } else {
        os << "uncrt_";
    }
    os << OutWidth << "_" << OutHeight << "_" << Crop.X << "_" << Crop.Y << "_" << Crop.Width << "_" << Crop.Height;
    for (int i = 0; i < 4; i++) {
        os << "_" << Quad[i].x << "_" << Quad[i].y;
    }
    for (int i = 0; i < 16; i++) {
        os << "_" << Patch[i].x << "_" << Patch[i].y;
    }
    return os.str();
}

CarbonConfig CarbonConfig::Defaults() {
    CarbonConfig cfg;
    cfg.StaticVideoThreadCfg = rgms::video::StaticVideoThreadConfig::Defaults();
    cfg.StaticVideoThreadCfg.StaticVideoBufferCfg.BufferSize = 1024u * 1024u * 1024u;
    cfg.FilterCfg = FilterConfig::Defaults();
    cfg.NumXGridDiv = 16;
    cfg.NumYGridDiv = 15;
    return cfg;
}

bool carbon::ParseArgumentsToConfig(int* argc, char*** argv, CarbonConfig* config) {
    if (*argc == 2) {
        util::ArgReadString(argc, argv, &config->InPath);
        util::ArgReadString(argc, argv, &config->OutPath);
        return true;
    }
    if (*argc == 3) {
        util::ArgReadString(argc, argv, &config->InPath);
        util::ArgReadString(argc, argv, &config->OutPath);
        std::string fmt;
        util::ArgReadString(argc, argv, &fmt);
        config->FilterCfg.FromString(fmt);

        return true;
    }
    return false;
}

CarbonApp::CarbonApp(CarbonConfig* config)
    : m_Config(config)
    , m_VideoFrame(0)
    , m_FrameMult(1.0)
    , m_OutMult(4.0)
    , m_SelectedHandle(-1)
    , m_QuadHandlesActive(true)
{
    m_VideoThread = std::make_unique<video::StaticVideoThread>(
        std::make_unique<video::CVVideoCaptureSource>(m_Config->InPath),
        m_Config->StaticVideoThreadCfg);
}

CarbonApp::~CarbonApp() {
    if (!m_Config->OutPath.empty()) {
        std::ofstream of(m_Config->OutPath);
        of << m_Config->FilterCfg.ToString();
    }
}

void CarbonApp::ReadFrame() {
    m_LiveInputFrame = m_VideoThread->GetFrame(m_VideoFrame);
    if (m_LiveInputFrame) {
        m_Image = cv::Mat(
                m_LiveInputFrame->Height,
                m_LiveInputFrame->Width,
                CV_8UC3,
                m_LiveInputFrame->Buffer);

        if (m_Config->FilterCfg.Type == FilterType::UNCRT && m_PixelContributions.empty()) {
            UpdatePixelContributions();
        }
        m_OutImage = ApplyFilter(m_Image, m_Config->FilterCfg, m_PixelContributions);

        if (!(m_OutImage.rows == 0 || m_OutImage.cols == 0)) {
            cv::resize(m_OutImage, m_OutImage, {}, m_OutMult, m_OutMult, cv::INTER_NEAREST);
        }

        m_Image = rgmui::CropWithZeroPadding(m_Image, 
                cv::Rect(-50, -50, m_Image.cols + 100, m_Image.rows + 100));
        if (m_FrameMult > 0 && m_FrameMult != 1.0) {
            cv::resize(m_Image, m_Image, {}, m_FrameMult, m_FrameMult);
        }
    }
}

void CarbonApp::SelectHandleHotkeys() {
    FilterType ft = m_Config->FilterCfg.Type;
    if (ft == FilterType::CROP || ft == FilterType::PERSPECTIVE) {
        if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_1)) {
            m_SelectedHandle = 3;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_2)) {
            m_SelectedHandle = 5;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_3)) {
            m_SelectedHandle = 2;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_4)) {
            m_SelectedHandle = 6;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_5)) {
            m_SelectedHandle = 8;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_6)) {
            m_SelectedHandle = 7;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_7)) {
            m_SelectedHandle = 0;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_8)) {
            m_SelectedHandle = 4;
        } else if (ImGui::IsKeyPressed(SDL_SCANCODE_KP_9)) {
            m_SelectedHandle = 1;
        }
    }
}

void CarbonApp::UpdatePixelContributions() {
    if (m_LiveInputFrame) {
        rgms::video::ComputePixelContributions(
            m_LiveInputFrame->Height, m_LiveInputFrame->Width, 
            m_Config->FilterCfg.Patch,
            &m_PixelContributions, 
            m_Config->FilterCfg.OutWidth,
            m_Config->FilterCfg.OutHeight);
    }
}


bool CarbonApp::OnFrame() {
    bool changed = !m_LiveInputFrame;
    bool uncrtchanged = false;
    if (ImGui::Begin("filter")) {
        FilterConfig* filter = &m_Config->FilterCfg;

        bool typeChanged = false;
        if (ImGui::RadioButton("None", filter->Type == FilterType::NONE)) {
            filter->Type = FilterType::NONE;
            changed = true;
            typeChanged = true;
        }
        if (ImGui::RadioButton("Crop", filter->Type == FilterType::CROP)) {
            filter->Type = FilterType::CROP;
            changed = true;
            typeChanged = true;
        }
        if (ImGui::RadioButton("Perspective", filter->Type == FilterType::PERSPECTIVE)) {
            filter->Type = FilterType::PERSPECTIVE;
            changed = true;
            typeChanged = true;
        }
        if (ImGui::RadioButton("Uncrt", filter->Type == FilterType::UNCRT)) {
            filter->Type = FilterType::UNCRT;
            uncrtchanged = true;
            typeChanged = true;
        }

        if (typeChanged) {
            m_SelectedHandle = -1;
        }

        ImGui::PushItemWidth(120);
        if (filter->Type == FilterType::PERSPECTIVE) {
            ImGui::Text("Quad");
            for (int i = 0; i < 4; i++) {
                changed |= ImGui::InputFloat2(fmt::format("{}", i).c_str(), filter->Quad[i].data.data(), "%.0f");
            }
            if (m_LiveInputFrame) {
                if (ImGui::Button("set perspective crop")) {
                    changed = true;
                    filter->Crop.X = 0;
                    filter->Crop.Y = 0;
                    filter->Crop.Width = m_LiveInputFrame->Width;
                    filter->Crop.Height = m_LiveInputFrame->Height;
                }
                ImGui::Checkbox("quad handles", &m_QuadHandlesActive);
            }
        }

        if (filter->Type == FilterType::CROP || filter->Type == FilterType::PERSPECTIVE) {
            ImGui::Text("Crop");
            changed |= ImGui::InputFloat("x", &filter->Crop.X);
            changed |= ImGui::InputFloat("y", &filter->Crop.Y);
            changed |= ImGui::InputFloat("width", &filter->Crop.Width);
            changed |= ImGui::InputFloat("height", &filter->Crop.Height);

        }

        if (filter->Type == FilterType::UNCRT) {
            ImGui::Text("Patch");
            int i = 0;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    uncrtchanged |= ImGui::InputFloat2(fmt::format("{:02d}", i).c_str(), filter->Patch[i].data.data(), "%.0f");
                    i++;
                }
            }
            ImGui::PopItemWidth();
        }
    }
    ImGui::End();

    if (ImGui::Begin("in frame")) {
        ImGui::PushItemWidth(300);
        if (rgmui::SliderIntExt("frame", &m_VideoFrame, 0, m_VideoThread->CurrentKnownNumFrames() - 1)) {
            changed = true;
        }

        if (rgmui::SliderFloatExt("mult", &m_FrameMult, 0.1, 3.0)) {
            changed = true;
        }
        ImGui::PopItemWidth();

        if (m_LiveInputFrame) {
            ImGui::TextUnformatted(fmt::format("{}x{} {:7d} {}", 
                        m_LiveInputFrame->Width, m_LiveInputFrame->Height, m_LiveInputFrame->FrameNumber,
                        util::SimpleMillisFormat(m_LiveInputFrame->PtsMilliseconds, util::SimpleTimeFormatFlags::MSCS)).c_str());
            rgmui::MatAnnotator mat("frame", m_Image, m_FrameMult, 
                    util::Vector2F(-50 * m_FrameMult, -50 * m_FrameMult), false);

            DrawFilterAnnotations(&mat, m_Config->FilterCfg);
            SetupHandles();
            SelectHandleHotkeys();


            if (m_SelectedHandle != -1) {
                int dx, dy;
                if (rgms::rgmui::ArrowKeyHelperInFrame(&dx, &dy, 8)) {
                    m_Handles[m_SelectedHandle]->Shift(util::Vector2F(dx, dy));
                    changed = true;
                    if (m_Config->FilterCfg.Type == FilterType::UNCRT) {
                        uncrtchanged = true;
                    }
                }
            }



            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {

                m_SelectedHandle = -1;
                ImVec2 sp = ImGui::GetMousePos();
                for (int i = 0; i < static_cast<int>(m_Handles.size()); i++) {
                    auto& handle = m_Handles[i];
                    util::Vector2F a = handle->HandleLocation();
                    util::Vector2F s = handle->HandleSize() + 5;
                    a -= s / 2;

                    ImVec2 hl = mat.MatPosToScreenPos(a);
                    ImVec2 hb = mat.MatPosToScreenPos(a + s);

                    if (sp.x >= hl.x && sp.y >= hl.y && sp.x <= hb.x && sp.y <= hb.y) {
                        m_SelectedHandle = i;
                        break;
                    }
                }
            }

            if (m_SelectedHandle >= 0 && 
                ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {

                ImVec2 sp = ImGui::GetMousePos();
                auto p = mat.ScreenPosToMatPos2F(sp);
                m_Handles[m_SelectedHandle]->MoveTo(p);
                changed = true;
                if (m_Config->FilterCfg.Type == FilterType::UNCRT) {
                    uncrtchanged = true;
                }
            }

            DrawHandles(&mat);
        }
    }
    ImGui::End();

    if (ImGui::Begin("out frame")) {
        if (m_LiveInputFrame) {
            rgmui::MatAnnotator mat("out", m_OutImage, m_OutMult); 
            // TODO account for xdiv etc
            for (int x = 0; x < 256; x += 8) {
                int w = 1;
                if (x % 16 == 0) {
                    w = 2;
                }
                mat.AddLine(util::Vector2F(x, 0), util::Vector2F(x, 240), IM_COL32(255, 0, 0, 255), w);
            }
            for (int y = 0; y < 240; y += 8) {
                int w = 1;
                if (y % 16 == 0) {
                    w = 2;
                }
                mat.AddLine(util::Vector2F(0, y), util::Vector2F(256, y), IM_COL32(255, 0, 0, 255), w);
            }
        }
    }
    ImGui::End();

    if (uncrtchanged) {
        UpdatePixelContributions();
    }

    if (changed || uncrtchanged) {
        ReadFrame();
    }
    return true;
}

void CarbonApp::DrawFilterAnnotations(rgms::rgmui::MatAnnotator* mat,
        const FilterConfig& filter) {
    ImU32 col = IM_COL32(255, 0, 0, 255);
    ImU32 col2 = IM_COL32(0, 255, 0, 255);

    if (filter.Type == FilterType::CROP) {
        util::Vector2F a = filter.Crop.TopLeft();
        util::Vector2F b = filter.Crop.BottomRight();
        for (auto x : util::Linspace(a.x, b.x, m_Config->NumXGridDiv + 1)) {
            mat->AddLine({x, a.y}, {x, b.y}, col);
        }
        for (auto y : util::Linspace(a.y, b.y, m_Config->NumYGridDiv + 1)) {
            mat->AddLine({a.x, y}, {b.x, y}, col);
        }
    } else if (filter.Type == FilterType::PERSPECTIVE) {
        cv::Mat m = GetFilterPerspectiveMatrix(m_LiveInputFrame->Width, m_LiveInputFrame->Height,
                filter).inv(); // NOTE inv

        std::vector<cv::Point2f> vec;
        for (auto t : util::Linspace(0.0, 1.0, m_Config->NumXGridDiv + 1)) {
            float xp = filter.Crop.X + filter.Crop.Width * t;
            vec.push_back(cv::Point2f(xp, filter.Crop.Y));
            vec.push_back(cv::Point2f(xp, filter.Crop.Y + filter.Crop.Height));
        }
        for (auto t : util::Linspace(0.0, 1.0, m_Config->NumYGridDiv + 1)) {
            float yp = filter.Crop.Y + filter.Crop.Height * t;
            vec.push_back(cv::Point2f(filter.Crop.X, yp));
            vec.push_back(cv::Point2f(filter.Crop.X + filter.Crop.Width, yp));
        }


        cv::perspectiveTransform(vec, vec, m);
        for(int i = 0; i < vec.size(); i+=2) {
            int j = i + 1;
            mat->AddLine(util::Vector2F(vec[i].x, vec[i].y), util::Vector2F(vec[j].x, vec[j].y), col);
        }

        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            mat->AddLine(filter.Quad[i], filter.Quad[j], col2, 1);
        }
    } else if (filter.Type == FilterType::UNCRT) {
        auto& patch = filter.Patch;

        for (auto x : util::Linspace(0.0, 1.0, m_Config->NumXGridDiv + 1)) {
            util::Vector2F a = util::EvaluateBezier(x, patch[0], patch[1], patch[2], patch[3]);
            util::Vector2F b = util::EvaluateBezier(x, patch[4], patch[5], patch[6], patch[7]);
            util::Vector2F c = util::EvaluateBezier(x, patch[8], patch[9], patch[10], patch[11]);
            util::Vector2F d = util::EvaluateBezier(x, patch[12], patch[13], patch[14], patch[15]);

            mat->AddBezierCubic(a, b, c, d, col);
        }
        for (auto y : util::Linspace(0.0, 1.0, m_Config->NumYGridDiv + 1)) {
            util::Vector2F a = util::EvaluateBezier(y, patch[0], patch[4], patch[8], patch[12]);
            util::Vector2F b = util::EvaluateBezier(y, patch[1], patch[5], patch[9], patch[13]);
            util::Vector2F c = util::EvaluateBezier(y, patch[2], patch[6], patch[10], patch[14]);
            util::Vector2F d = util::EvaluateBezier(y, patch[3], patch[7], patch[11], patch[15]);

            mat->AddBezierCubic(a, b, c, d, col);
        }
    }
}

rgms::util::Vector2F CarbonApp::HandleSize(int x, int y) {
    float dx = (x == 1) ? 10.0f : 15.0f;
    float dy = (y == 1) ? 10.0f : 15.0f;

    dx /= m_FrameMult;
    dy /= m_FrameMult;

    return util::Vector2F(dx, dy);
}

void CarbonApp::AddCropHandles(cv::Mat m) {
    FilterConfig& filter = m_Config->FilterCfg;
    ImU32 c = IM_COL32(255, 0, 0, 255);

    m_Handles.push_back(std::make_shared<CropHandle>(
        filter.Crop.X, filter.Crop.Y, 
        [&](float sx, float sy){ 
            filter.Crop.Width -= (sx - filter.Crop.X);
            filter.Crop.Height -= (sy - filter.Crop.Y);
            filter.Crop.X = sx;
            filter.Crop.Y = sy;
        },
        [&](float dx, float dy){ 
            filter.Crop.X += dx;
            filter.Crop.Width -= dx;
            filter.Crop.Y += dy;
            filter.Crop.Height -= dy;
        }, c, HandleSize(1, 1), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X + filter.Crop.Width), (filter.Crop.Y),
            [&](float sx, float sy){ 
                filter.Crop.Width = sx - filter.Crop.X;
                filter.Crop.Height -= (sy - filter.Crop.Y);
                filter.Crop.Y = sy;
            },
            [&](float dx, float dy){ 
                filter.Crop.Width += dx;
                filter.Crop.Y += dy;
                filter.Crop.Height -= dy;
            }, c, HandleSize( 1, 1), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X + filter.Crop.Width), (filter.Crop.Y + filter.Crop.Height),
            [&](float sx, float sy){ 
                filter.Crop.Width = sx - filter.Crop.X;
                filter.Crop.Height = sy - filter.Crop.Y;
            },
            [&](float dx, float dy){ 
                filter.Crop.Width += dx;
                filter.Crop.Height += dy;
            }, c, HandleSize( 1, 1), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X), (filter.Crop.Y + filter.Crop.Height),
            [&](float sx, float sy){ 
                filter.Crop.Height = (sy - filter.Crop.Y);
                filter.Crop.Width -= (sx - filter.Crop.X);
                filter.Crop.X = sx;
            },
            [&](float dx, float dy){ 
                filter.Crop.Height += dy;
                filter.Crop.X += dx;
                filter.Crop.Width -= dx;
            }, c, HandleSize( 1, 1), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X + filter.Crop.Width / 2), (filter.Crop.Y),
            [&](float sx, float sy){ 
                filter.Crop.Height -= (sy - filter.Crop.Y);
                filter.Crop.Y = sy;
            },
            [&](float dx, float dy){ 
                filter.Crop.Height -= dy;
                filter.Crop.Y += dy;
            }, c, HandleSize( 2, 1), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X + filter.Crop.Width / 2), (filter.Crop.Y + filter.Crop.Height),
            [&](float sx, float sy){ 
                filter.Crop.Height = sy - filter.Crop.Y;
            },
            [&](float dx, float dy){ 
                filter.Crop.Height += dy;
            }, c, HandleSize( 2, 1), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X), (filter.Crop.Y + filter.Crop.Height/ 2),
            [&](float sx, float sy){ 
                filter.Crop.Width -= (sx - filter.Crop.X);
                filter.Crop.X = sx;
            },
            [&](float dx, float dy){ 
                filter.Crop.X += dx;
                filter.Crop.Width -= dx;
            }, c, HandleSize( 1, 2), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X + filter.Crop.Width), (filter.Crop.Y + filter.Crop.Height/ 2),
            [&](float sx, float sy){ 
                filter.Crop.Width = sx - filter.Crop.X;
            },
            [&](float dx, float dy){ 
                filter.Crop.Width += dx;
            }, c, HandleSize( 1, 2), m));
    m_Handles.push_back(std::make_shared<CropHandle>(
                (filter.Crop.X + filter.Crop.Width / 2), (filter.Crop.Y + filter.Crop.Height/ 2),
            [&](float sx, float sy){ 
                filter.Crop.X = (sx - filter.Crop.Width / 2);
                filter.Crop.Y = (sy - filter.Crop.Height / 2);
            },
            [&](float dx, float dy){ 
                filter.Crop.X += dx;
                filter.Crop.Y += dy;
            }, c, HandleSize( 2, 2), m));
}

void CarbonApp::SetupHandles() {
    m_Handles.clear();

    FilterConfig& filter = m_Config->FilterCfg;
    if (filter.Type == FilterType::CROP) {
        cv::Mat m;
        AddCropHandles(m);
    } else if (filter.Type == FilterType::PERSPECTIVE) {
        if (m_QuadHandlesActive) {
            ImU32 c = IM_COL32(0, 255, 0, 255);
            std::vector<util::Vector2F*> apts;
            for (int i = 0; i < 4; i++) {
                m_Handles.push_back(std::make_shared<PointsHandle>(&filter.Quad[i], c, HandleSize(1, 1)));
                std::vector<util::Vector2F*> pts = {
                    &filter.Quad[i],
                    &filter.Quad[(i + 1) % 4]
                };
                m_Handles.push_back(std::make_shared<PointsHandle>(pts, c, 
                            HandleSize(1 + ((i + 1) % 2), 1 + (i % 2))));
                apts.push_back(&filter.Quad[i]);
            }
            m_Handles.push_back(std::make_shared<PointsHandle>(apts, c, HandleSize(2, 2)));

            // just whatever dude
            std::swap(m_Handles[4], m_Handles[1]);
            std::swap(m_Handles[2], m_Handles[1]);
            std::swap(m_Handles[6], m_Handles[7]);
            std::swap(m_Handles[3], m_Handles[7]);

        } else {
            cv::Mat m = GetFilterPerspectiveMatrix(m_LiveInputFrame->Width, m_LiveInputFrame->Height,
                    filter).inv(); // NOTE inv
            AddCropHandles(m);
        }
    } else if(filter.Type == FilterType::UNCRT) {
        ImU32 c = IM_COL32(255, 0, 0, 255);
        ImU32 c2 = IM_COL32(170, 0, 0, 255);

        std::vector<util::Vector2F*> apts;
        for (int i = 0; i < 16; i++) {
            m_Handles.push_back(std::make_shared<PointsHandle>(&filter.Patch[i], c, HandleSize(1, 1)));
            apts.push_back(&filter.Patch[i]);
        }
        m_Handles.push_back(std::make_shared<BezierHandle>(apts, c2, HandleSize(2, 2)));

        for (int i = 0; i < 4; i++) {
            std::vector<util::Vector2F*> pts = {
                &filter.Patch[i * 4 + 0],
                &filter.Patch[i * 4 + 1],
                &filter.Patch[i * 4 + 2],
                &filter.Patch[i * 4 + 3],
            };

            m_Handles.push_back(std::make_shared<BezierHandle>(pts, c2, HandleSize(2, 1)));
        }
        for (int i = 0; i < 4; i++) {
            std::vector<util::Vector2F*> pts = {
                &filter.Patch[i + 0 * 4],
                &filter.Patch[i + 1 * 4],
                &filter.Patch[i + 2 * 4],
                &filter.Patch[i + 3 * 4],
            };

            m_Handles.push_back(std::make_shared<BezierHandle>(pts, c2, HandleSize(1, 2)));
        }
    }
}

void CarbonApp::DrawHandles(rgms::rgmui::MatAnnotator* mat) {
    int index = 0;
    for (auto & handle : m_Handles) {
        util::Vector2F a = handle->HandleLocation();
        util::Vector2F s = handle->HandleSize();
        a -= s / 2;

        ImU32 col = (index == m_SelectedHandle) ? IM_COL32_WHITE : handle->HandleColor();
        mat->AddRectFilled(a, a + s, col);


        index += 1;
    }

}

////////////////////////////////////////////////////////////////////////////////

IHandle::IHandle() {
}

IHandle::~IHandle() {
}

////////////////////////////////////////////////////////////////////////////////

PointsHandle::PointsHandle(util::Vector2F* pt, ImU32 c, util::Vector2F size)
    : m_Color(c)
    , m_Size(size)

{
    m_Points.push_back(pt);
}


PointsHandle::PointsHandle(std::vector<util::Vector2F*> pts, ImU32 c, util::Vector2F size)
    : m_Points(pts)
    , m_Color(c)
    , m_Size(size)
{
}

PointsHandle::~PointsHandle() {
}

void PointsHandle::MoveTo(const util::Vector2F& v) {
    util::Vector2F a = HandleLocation();
    Shift(v - a);
}

void PointsHandle::Shift(const util::Vector2F& o) {
    for (auto & pt : m_Points) {
        (*pt) += o;
    }
}

rgms::util::Vector2F PointsHandle::HandleLocation() const {
    if (m_Points.size() == 1) {
        return *m_Points[0];
    }

    util::Vector2F a = util::Vector2F::Origin();
    float n = 0;
    for (auto & pt : m_Points) {
        a += *pt;
        n += 1.0f;
    }
    if (n) {
        a /= n;
    }
    return a;
}

rgms::util::Vector2F PointsHandle::HandleSize() const {
    return m_Size;
}

ImU32 PointsHandle::HandleColor() const {
    return m_Color;
}

////////////////////////////////////////////////////////////////////////////////

BezierHandle::BezierHandle(std::vector<util::Vector2F*> pts, ImU32 c, util::Vector2F size)
    : PointsHandle(pts, c, size)
{
}

BezierHandle::~BezierHandle() {
}

util::Vector2F BezierHandle::HandleLocation() const {
    if (m_Points.size() == 4) {
        return util::EvaluateBezier(0.5, *m_Points[0], *m_Points[1], *m_Points[2], *m_Points[3]);
    } else if (m_Points.size() == 16) {
        auto a = util::EvaluateBezier(0.5, *m_Points[0], *m_Points[1], *m_Points[2], *m_Points[3]);
        auto b = util::EvaluateBezier(0.5, *m_Points[4], *m_Points[5], *m_Points[6], *m_Points[7]);
        auto c = util::EvaluateBezier(0.5, *m_Points[8], *m_Points[9], *m_Points[10], *m_Points[11]);
        auto d = util::EvaluateBezier(0.5, *m_Points[12], *m_Points[13], *m_Points[14], *m_Points[15]);
        return util::EvaluateBezier(0.5, a, b, c, d);
    }

    return PointsHandle::HandleLocation();
}

CropHandle::CropHandle(float x, float y,
            std::function<void(float sx, float sy)> moveTo,
            std::function<void(float dx, float dy)> shift,
            ImU32 c, rgms::util::Vector2F size, cv::Mat m)
    : m_X(x)
    , m_Y(y)
    , m_MoveTo(moveTo)
    , m_Shift(shift)
    , m_Color(c)
    , m_Size(size)
    , m_Transform(m)
{
}

CropHandle::~CropHandle() {
}

void CropHandle::MoveTo(const rgms::util::Vector2F& v) {
    std::vector<cv::Point2f> vec = {
        {v.x, v.y}
    };
    if (m_Transform.rows == 3) {
        cv::perspectiveTransform(vec, vec, m_Transform.inv());
    }
    m_MoveTo(vec[0].x, vec[0].y);
}
void CropHandle::Shift(const rgms::util::Vector2F& o) {
    m_Shift(o.x, o.y);
}
rgms::util::Vector2F CropHandle::HandleLocation() const {
    std::vector<cv::Point2f> vec = {
        {m_X, m_Y}
    };
    if (m_Transform.rows == 3) {
        cv::perspectiveTransform(vec, vec, m_Transform);
    }

    return rgms::util::Vector2F(vec[0].x, vec[0].y);
}
rgms::util::Vector2F CropHandle::HandleSize() const {
    return m_Size;
}
ImU32 CropHandle::HandleColor() const {
    return m_Color;
}

