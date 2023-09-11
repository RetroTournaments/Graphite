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

//#include "fmt/core.h"

#include "rgmutil/util.h"
#include "rgmvideo/uncrt.h"

using namespace rgms;

void Usage(std::ostream& os) {
    os << "usage: uncrt [options] [--directory dir] [image.png ...]" << std::endl;
    os << " --help                  : print usage and exit" << std::endl;
    os << " --mesh mesh_points      : the 16 point bezier mesh" << std::endl;
    os << "     mesh_points as      : x0:y0:x1:y1:...:x15:y15" << std::endl;
    os << " --inplace               : process the images in place overwriting them" << std::endl;
    os << " --directory directory/  : optional directories to process" << std::endl;
}

bool ParsePatch(const std::string& meshPoints, util::BezierPatch* patch) {
    std::istringstream is(meshPoints);
    int patchIndex = 0;
    try {
        for (std::string v; std::getline(is, v, ':'); patchIndex++) {
            if (patchIndex >= 32) {
                return false;
            }
            
            (*patch)[patchIndex / 2][patchIndex % 2] = std::stof(v);
        }

    } catch (std::invalid_argument e) {
        return false;
    }

    return patchIndex == 32;
}

int main(int argc, char** argv) {
    util::ArgNext(&argc, &argv); // Skip path argument

    if (argc <= 0) {
        Usage(std::cerr);
        return 1;
    }

    std::vector<std::string> images;

    bool inplace = false;
    bool readingImages = false;

    bool patchInitialized = false;
    util::BezierPatch bp;

    // TODO arguments to control these?
    int outx = 256;
    int outy = 240;

    std::string arg;
    while (util::ArgReadString(&argc, &argv, &arg)) {
        if (!readingImages) {
            if (arg == "--help") {
                Usage(std::cout);
                return 0;
            } else if (arg == "--mesh") {
                if (!util::ArgReadString(&argc, &argv, &arg) || !ParsePatch(arg, &bp)) {
                    std::cerr << "Failure parsing mesh_points" << std::endl << std::endl;
                    Usage(std::cerr);
                    return 1;
                }
                patchInitialized = true;
            } else if (arg == "--inplace") {
                inplace = true;
            } else if (arg == "--directory") {
                if (!util::ArgReadString(&argc, &argv, &arg)) {
                    std::cerr << "Failure reading directory" << std::endl << std::endl;
                    Usage(std::cerr);
                    return 1;
                }
                util::ForFileInDirectory(arg, [&](util::fs::path p){
                    images.push_back(p.string());
                    return true;
                });
            } else {
                readingImages = true;
            }
        }

        if (readingImages) {
            images.push_back(arg);
        }
    }

    if (!patchInitialized) {
        std::cerr << "Must supply mesh" << std::endl << std::endl;
        Usage(std::cerr);
        return 1;
    }
    if (!inplace) {
        std::cerr << "Must be inplace for now because I haven't written the rest >_>" << std::endl << std::endl;
        return 1;
    }

    if (images.empty()) {
        return 0;
    }

    std::unordered_map<int, std::unordered_map<int, video::PixelContributions>> pcontrib;

    size_t i = 0;
    for (auto & imgPath : images) {
        cv::Mat m = cv::imread(imgPath);
        if (m.empty() || m.rows == 0 || m.cols == 0) {
            std::cerr << "Invalid imgpath: " << imgPath << std::endl;
            return 1;
        }

        auto& pmap = pcontrib[m.cols];
        auto it = pmap.find(m.rows);
        video::PixelContributions* thisContrib;

        if (it == pmap.end()) {
            std::cout << "computing pixel contributions: " << m.rows << "x" << m.cols << std::endl;
            thisContrib = &pmap[m.rows];
            video::ComputePixelContributions(m.rows, m.cols, bp, 
                    thisContrib, outx, outy);
        } else {
            thisContrib = &it->second;
        }

        cv::Mat u = video::RemoveCRT(m, *thisContrib, outx, outy);
        cv::imwrite(imgPath, u);
        i++;
        std::cout << i << " " << images.size() << "\r" << std::flush;
        //fmt::print(" {:7d} / {:7d} \r", i, images.size());
    }
    std::cout << std::endl;

    return 0;
}
