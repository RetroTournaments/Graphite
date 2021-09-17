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

#ifndef RGMS_UNCRT_HEADER
#define RGMS_UNCRT_HEADER

#include "opencv2/opencv.hpp"

#include "rgmutil/util.h"

namespace rgms::video {

typedef std::vector<std::vector<std::pair<size_t, float>>> PixelContributions;

void ComputePixelContributions(int rows, int cols, const util::BezierPatch& patch,
    PixelContributions* contrib, int outx, int outy);

cv::Mat RemoveCRT(const cv::Mat& frame, const PixelContributions& contrib, 
        int outx, int outy);

}

#endif

