////////////////////////////////////////////////////////////////////////////////
//
// The Carbon library contains the application, and the components which make
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

#ifndef CARBON_CARBON_HEADER
#define CARBON_CARBON_HEADER

#include <initializer_list>

#include "rgmui/rgmui.h"
#include "rgmvideo/video.h"
#include "rgmvideo/uncrt.h"

namespace carbon {

enum class FilterType {
    NONE,
    CROP,
    PERSPECTIVE,
    UNCRT
};

struct FilterConfig {
    FilterType Type;
    rgms::util::Rect2F Crop;
    rgms::util::Quadrilateral2F Quad;
    rgms::util::BezierPatch Patch;
    int OutWidth;
    int OutHeight;

    void FromString(const std::string& str);
    std::string ToString() const;

    static FilterConfig Defaults();
};
cv::Mat ApplyFilter(const cv::Mat img, const FilterConfig& filter,
        const rgms::video::PixelContributions& contributions);
cv::Mat GetFilterPerspectiveMatrix(int width, int height, const FilterConfig& filter);

struct CarbonConfig {
    std::string InPath;
    std::string OutPath;
    FilterConfig FilterCfg;
    int NumXGridDiv;
    int NumYGridDiv;

    rgms::video::StaticVideoThreadConfig StaticVideoThreadCfg;


    static CarbonConfig Defaults();
};
bool ParseArgumentsToConfig(int* argc, char*** argv, CarbonConfig* config);

class IHandle {
public:
    IHandle();
    virtual ~IHandle();

    virtual void MoveTo(const rgms::util::Vector2F& v) = 0;
    virtual void Shift(const rgms::util::Vector2F& d) = 0;
    virtual rgms::util::Vector2F HandleLocation() const = 0;
    virtual rgms::util::Vector2F HandleSize() const = 0;
    virtual ImU32 HandleColor() const = 0;
};

class PointsHandle : public IHandle {
public:
    PointsHandle(rgms::util::Vector2F* pt, ImU32 c, rgms::util::Vector2F size);
    PointsHandle(std::vector<rgms::util::Vector2F*> points, ImU32 c, rgms::util::Vector2F size);
    virtual ~PointsHandle();

    virtual void MoveTo(const rgms::util::Vector2F& v) override final;
    virtual void Shift(const rgms::util::Vector2F& o) override final;
    virtual rgms::util::Vector2F HandleLocation() const override;
    virtual rgms::util::Vector2F HandleSize() const override final;
    virtual ImU32 HandleColor() const override final;

protected:
    std::vector<rgms::util::Vector2F*> m_Points;
    ImU32 m_Color;
    rgms::util::Vector2F m_Size;
};

class BezierHandle : public PointsHandle {
public:
    BezierHandle(std::vector<rgms::util::Vector2F*> points, ImU32 c, rgms::util::Vector2F size);
    virtual ~BezierHandle();

    virtual rgms::util::Vector2F HandleLocation() const override final;
};

class CropHandle : public IHandle {
public:
    CropHandle(
            float x, float y,
            std::function<void(float sx, float sy)> moveTo,
            std::function<void(float dx, float dy)> shift,
            ImU32 c, rgms::util::Vector2F size, cv::Mat t = cv::Mat());
    virtual ~CropHandle();

    virtual void MoveTo(const rgms::util::Vector2F& v) override final;
    virtual void Shift(const rgms::util::Vector2F& o) override final;
    virtual rgms::util::Vector2F HandleLocation() const override;
    virtual rgms::util::Vector2F HandleSize() const override final;
    virtual ImU32 HandleColor() const override final;

private:
    float m_X, m_Y;
    std::function<void(float, float)> m_MoveTo;
    std::function<void(float, float)> m_Shift;
    ImU32 m_Color;
    rgms::util::Vector2F m_Size;
    cv::Mat m_Transform;
};


class CarbonApp : public rgms::rgmui::IApplication {
public:
    CarbonApp(CarbonConfig* config);
    ~CarbonApp();

    virtual bool OnFrame() override;

private:
    void ReadFrame();
    void DrawFilterAnnotations(
            rgms::rgmui::MatAnnotator* mat,
            const FilterConfig& filter);
    void SetupHandles();
    void AddCropHandles(cv::Mat m);
    void DrawHandles(rgms::rgmui::MatAnnotator* mat);
    void SelectHandleHotkeys();
    rgms::util::Vector2F HandleSize(int x, int y);
    void UpdatePixelContributions();

private:
    CarbonConfig* m_Config;
    int m_VideoFrame;
    float m_FrameMult;
    float m_OutMult;

    std::vector<std::shared_ptr<IHandle>> m_Handles;
    int m_SelectedHandle;
    bool m_QuadHandlesActive;

    std::unique_ptr<rgms::video::StaticVideoThread> m_VideoThread;
    rgms::video::LiveInputFramePtr m_LiveInputFrame;
    rgms::video::PixelContributions m_PixelContributions;
    cv::Mat m_Image;
    cv::Mat m_OutImage;
};


}

#endif

