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

#include "rgmvideo/uncrt.h"

using namespace rgms::video;
using namespace rgms::util;


static bool PointInFour(const Vector2F& pt, const std::array<Vector2F, 5>& poly) {
    int wn = 0;
    for (int i = 0; i < 4; i++) {
        if (poly[i].y <= pt.y) {
            if (poly[i+1].y > pt.y) {
                if (IsLeft(poly[i], poly[i+1], pt)) {
                    wn++;
                }
            }
        } else {
            if (poly[i+1].y <= pt.y) {
                if (IsRight(poly[i], poly[i+1], pt)) {
                    wn--;
                }
            }
        }
    }
    return wn != 0;
}

static float PolyPixOver(int x, int y, const std::array<Vector2F, 5>& poly) {
    // could change to 5 and
    // -0.4, -0.2, 0, 0.2, 0.4
    int n = 3;
    std::array<float, 3> d {-0.33333333333f, 0.0f, 0.33333333333f};

    float xf = static_cast<float>(x);
    float yf = static_cast<float>(y);

    int cnt = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            Vector2F p(xf + d[j], yf + d[i]);
            if (PointInFour(p, poly)) {
                cnt++;
            }
        }
    }

    return static_cast<float>(cnt) / static_cast<float>(n * n);
}

void rgms::video::ComputePixelContributions(int rows, int cols,
        const BezierPatch& patch, PixelContributions* contrib, int outx, int outy) {

    auto ac = [&](float t){
        return EvaluateBezier(t, patch[ 0], patch[ 4], patch[ 8], patch[12]);
    };
    auto bc = [&](float t){
        return EvaluateBezier(t, patch[ 1], patch[ 5], patch[ 9], patch[13]);
    };
    auto cc = [&](float t){
        return EvaluateBezier(t, patch[ 2], patch[ 6], patch[10], patch[14]);
    };
    auto dc = [&](float t){
        return EvaluateBezier(t, patch[ 3], patch[ 7], patch[11], patch[15]);
    };

    std::vector<Vector2F> coords((outx + 1) * (outy + 1));
    size_t i = 0;
    for (float yt : Linspace(0.0f, 1.0f, outy + 1)) {
        Vector2F a = ac(yt);
        Vector2F b = bc(yt);
        Vector2F c = cc(yt);
        Vector2F d = dc(yt);

        for (float xt : Linspace(0.0f, 1.0f, outx + 1)) {
            coords[i] = EvaluateBezier(xt, a, b, c, d);
            i++;
        }
    }

    contrib->resize(outy * outx);
    i = 0;
    for (int y = 0; y < outy; y++) {
        for (int x = 0; x < outx; x++) {
            std::vector<std::pair<size_t, float>>& thisAmts = (*contrib)[i++];
            thisAmts.clear();

            int q = y * (outx + 1) + x;
            int j = (y + 1) * (outx + 1) + x;
            const Vector2F& a = coords[q];
            const Vector2F& b = coords[q+1];
            const Vector2F& c = coords[j+1];
            const Vector2F& d = coords[j];
            std::array<Vector2F, 5> poly {a, b, c, d, a};

            // a b
            // d c

            int sy = static_cast<int>(std::min(a.y, b.y));
            sy = std::max(sy, 0);
            int ey = static_cast<int>(std::max(c.y, d.y)) + 2;
            ey = std::min(ey, rows - 1);
            int sx = static_cast<int>(std::min(a.x, d.x));
            sx = std::max(sx, 0);
            int ex = static_cast<int>(std::max(b.x, c.x)) + 2;
            ex = std::min(ex, cols - 1);

            float total = 0.0f;
            for (int iny = sy; iny < ey; iny++) {
                for (int inx = sx; inx < ex; inx++) {
                    size_t origIndex = (iny * cols) + inx;

                    float amt = PolyPixOver(inx, iny, poly);
                    if (amt > 0.0f) {
                        total += amt;
                        thisAmts.emplace_back(origIndex, amt);
                    }
                }
            }
            if (total > 0) {
                for (auto & v : thisAmts) {
                    v.second /= total;
                }
            }
        }
    }
}

cv::Mat rgms::video::RemoveCRT(const cv::Mat& frame, const PixelContributions& contrib, int outx, int outy) {
    cv::Mat result(outy, outx, CV_32FC3);
    float* resPtr = reinterpret_cast<float*>(result.data);
    uint8_t* frPtr = reinterpret_cast<uint8_t*>(frame.data);

    size_t l = 0;
    for (int y = 0; y < outy; y++) {
        for (int x = 0; x < outx; x++) {
            resPtr[0] = 0.0f;
            resPtr[1] = 0.0f;
            resPtr[2] = 0.0f;
            for (auto & [q, a] : contrib.at(l++)) {
                uint8_t* tpix = frPtr + (q * 3);
                for (int i = 0; i < 3; i++) {
                    *(resPtr + i) += a * static_cast<float>(*(tpix + i));
                }
            }
            resPtr += 3;
        }
    }

    cv::Mat img;
    result.convertTo(img, CV_8UC3);
    return img;
}
