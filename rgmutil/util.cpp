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

#include <cmath>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "rgmutil/util.h"

using namespace rgms::util;

mclock::time_point rgms::util::Now() {
    return mclock::now();
}

int64_t rgms::util::ElapsedMillisFrom(const mclock::time_point& from) {
    return ElapsedMillis(from, Now());
}

int64_t rgms::util::ToMillis(const mclock::duration& duration) {
    double sec = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    return static_cast<int64_t>(std::round(sec * 1000));
}

mclock::duration rgms::util::ToDuration(int64_t millis) {
    return std::chrono::milliseconds(millis);
}

int64_t rgms::util::ElapsedMillis(const mclock::time_point& from, const mclock::time_point& to) {
    return ToMillis(to - from);
}

////////////////////////////////////////////////////////////////////////////////

std::string rgms::util::SimpleDurationFormat(const mclock::duration& duration, SimpleTimeFormatFlags flags) {
    return SimpleMillisFormat(ToMillis(duration), flags);
}

std::string rgms::util::SimpleMillisFormat(int64_t millis, SimpleTimeFormatFlags flags) {
    bool negative = millis < 0;
    millis = std::abs(millis);


    constexpr int64_t MILLIS_IN_SECONDS = 1000;
    constexpr int64_t MILLIS_IN_MINUTES = 60 * MILLIS_IN_SECONDS;
    constexpr int64_t MILLIS_IN_HOURS   = 60 * MILLIS_IN_MINUTES;
    constexpr int64_t MILLIS_IN_DAYS    = 24 * MILLIS_IN_HOURS;


    int64_t days = millis / MILLIS_IN_DAYS;
    millis -= days * MILLIS_IN_DAYS;
    int64_t hours = millis / MILLIS_IN_HOURS;
    millis -= hours * MILLIS_IN_HOURS;
    int64_t minutes = millis / MILLIS_IN_MINUTES;
    millis -= minutes * MILLIS_IN_MINUTES;
    int64_t seconds = millis / MILLIS_IN_SECONDS;
    millis -= seconds * MILLIS_IN_SECONDS;

    if (flags & SimpleTimeFormatFlags::CS) {
        if (millis >= 995) {
            seconds += 1;
            millis = 0;
        }
    }

    std::ostringstream os;
    if (negative) {
        os << "-";
    }

    auto OutNum = [&](int64_t num, int w) {
        os << std::setw(w) << std::setfill('0') << num;
    };
    if (flags & SimpleTimeFormatFlags::D) {
        OutNum(days, 2);
        os << ":";
    }
    if (flags & SimpleTimeFormatFlags::H) {
        if (flags & SimpleTimeFormatFlags::D) {
            OutNum(hours, 2);
        } else {
            OutNum(hours, 1);
        }
        os << ":";
    }
    if (flags & SimpleTimeFormatFlags::M) {
        if (flags & SimpleTimeFormatFlags::H) {
            OutNum(minutes, 2);
        } else {
            OutNum(minutes, 1);
        }
        os << ":";
    }
    if (flags & SimpleTimeFormatFlags::S) {
        if (!(flags & SimpleTimeFormatFlags::MS) &&
            !(flags & SimpleTimeFormatFlags::CS)) {
            if (millis >= 500) {
                seconds += 1;
            }
        }
        OutNum(seconds, 2);
    }

    if (flags & SimpleTimeFormatFlags::MS) {
        os << ".";
        OutNum(millis, 3);
    }
    else if (flags & SimpleTimeFormatFlags::CS) {
        os << ".";
        int cs = static_cast<int>(std::round(static_cast<float>(millis) / 10));
        OutNum(cs, 2);
    }
    return os.str();
}

////////////////////////////////////////////////////////////////////////////////

bool rgms::util::ArgPeekString(int* argc, char*** argv, std::string* str) {
    if (*argc <= 0) return false;
    *str = (*argv)[0];
    return true;
}

void rgms::util::ArgNext(int* argc, char*** argv) {
    (*argc)--;
    (*argv)++;
}

bool rgms::util::ArgReadString(int* argc, char*** argv, std::string* str) {
    if (!ArgPeekString(argc, argv, str)) return false;
    ArgNext(argc, argv);
    return true;
}

bool rgms::util::ArgReadInt(int* argc, char*** argv, int* v) {
    int64_t v64;
    if (ArgReadInt64(argc, argv, &v64)) {
        *v = static_cast<int>(v64);
        return true;
    }
    return false;
}

bool rgms::util::ArgReadInt64(int* argc, char*** argv, int64_t* v) {
    std::string arg;
    if (!ArgReadString(argc, argv, &arg)) return false;
    std::istringstream is(arg);
    int64_t q;
    is >> q;
    if (is.fail()) {
        return false;
    }
    *v = q;
    return true;
}

bool rgms::util::ArgReadDouble(int* argc, char*** argv, double* v) {
    std::string arg;
    if (!ArgReadString(argc, argv, &arg)) return false;

    std::istringstream is(arg);
    double q;
    is >> q;
    if (is.fail()) {
        return false;
    }
    *v = q;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

Vector2F rgms::util::EvaluateBezier(float t, const Vector2F& a, 
        const Vector2F& b, const Vector2F& c, const Vector2F& d) {
    float t2 = t * t;
    float t3 = t2 * t;
    float tm = 1.0f - t;
    float tm2 = tm * tm;
    float tm3 = tm2 * tm;
    return tm3 * a + (3.0f * tm2 * t) * b + (3.0f * tm * t2) * c + (t3) * d;
}

BezierPatch rgms::util::RectanglePatch(Vector2F origin, Vector2F size) {
    BezierPatch p;
    int i = 0;
    for (auto y : Linspace(origin.y, origin.y + size.y, 4)) {
        for (auto x : Linspace(origin.x, origin.x + size.x, 4)) {
            p[i].x = x;
            p[i].y = y;
            i++;
        }
    }
    return p;
}

////////////////////////////////////////////////////////////////////////////////

bool rgms::util::StringEndsWith(const std::string& str, const std::string& ending) {
    if (str.length() >= ending.length()) {
        return str.compare(str.length() - ending.length(), ending.length(), ending) == 0;
    } 
    return false;
}

bool rgms::util::StringStartsWith(const std::string& str, const std::string& start) {
    if (str.length() >= start.length()) {
        return str.compare(0, start.length(), start) == 0;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

void rgms::util::ForFileInDirectory(const std::string& directory,
        std::function<bool(fs::path p)> cback) {
    for (const auto & entry : fs::directory_iterator(directory)) {
        if (!cback(entry.path())) {
            return;
        }
    }
}

void rgms::util::ForFileOfExtensionInDirectory(const std::string& directory,
        const std::string& extension, std::function<bool(fs::path p)> cback) {
    for (const auto & entry : fs::directory_iterator(directory)) {
        if (entry.path().extension().string() == extension) {
            if (!cback(entry.path())) {
                return;
            }
        }
    }
}

bool rgms::util::FileExists(const std::string& path) {
    return fs::exists(path);
}

int rgms::util::ReadFileToVector(const std::string& path, std::vector<uint8_t>* contents) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.good()) {
        throw std::invalid_argument("ifstream not good");
    }
    *contents = std::move(std::vector<uint8_t>(
            std::istreambuf_iterator<char>(ifs), 
            std::istreambuf_iterator<char>()));
    return static_cast<int>(contents->size());
}

void rgms::util::WriteVectorToFile(const std::string& path, const std::vector<uint8_t>& contents) {
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (!ofs.good()) {
        throw std::invalid_argument("ofstream not good");
    }
    ofs.write(reinterpret_cast<const char*>(contents.data()), contents.size());
}

std::string rgms::util::ReadFileToString(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.good()) {
        throw std::invalid_argument("ifstream not good");
    }
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

////////////////////////////////////////////////////////////////////////////////

const std::vector<std::array<uint8_t, 3>>& rgms::util::GetColorMapColors(ColorMapType cmap) {
    static std::vector<std::array<uint8_t, 3>> BREWER_RDBU = {
        {0x67, 0x00, 0x1f},
        {0xb2, 0x18, 0x2b},
        {0xd6, 0x60, 0x4d},
        {0xf4, 0xa5, 0x82},
        {0xfd, 0xdb, 0xc7},
        {0xf7, 0xf7, 0xf7},
        {0xd1, 0xe5, 0xf0},
        {0x92, 0xc5, 0xde},
        {0x43, 0x93, 0xc3},
        {0x21, 0x66, 0xac},
        {0x05, 0x30, 0x61},
    };

    static std::vector<std::array<uint8_t, 3>> BREWER_YLORBR = {
        {0xff, 0xff, 0xe5},
        {0xff, 0xf7, 0xbc},
        {0xfe, 0xe3, 0x91},
        {0xfe, 0xc4, 0x4f},
        {0xfe, 0x99, 0x29},
        {0xec, 0x70, 0x14},
        {0xcc, 0x4c, 0x02},
        {0x99, 0x34, 0x04},
        {0x66, 0x25, 0x06},
    };
    static std::vector<std::array<uint8_t, 3>> BREWER_BLUES = {
        {0xf7, 0xfb, 0xff},
        {0xde, 0xeb, 0xf7},
        {0xc6, 0xdb, 0xef},
        {0x9e, 0xca, 0xe1},
        {0x6b, 0xae, 0xd6},
        {0x42, 0x92, 0xc6},
        {0x21, 0x71, 0xb5},
        {0x08, 0x51, 0x9c},
        {0x08, 0x30, 0x6b},
    };
    static std::vector<std::array<uint8_t, 3>> BREWER_REDS = {
        {0xff, 0xf5, 0xf0},
        {0xfe, 0xe0, 0xd2},
        {0xfc, 0xbb, 0xa1},
        {0xfc, 0x92, 0x72},
        {0xfb, 0x6a, 0x4a},
        {0xef, 0x3b, 0x2c},
        {0xcb, 0x18, 0x1d},
        {0xa5, 0x0f, 0x15},
        {0x67, 0x00, 0x0d},
    };
    static std::vector<std::array<uint8_t, 3>> BREWER_GREENS = {
        {0xf7, 0xfc, 0xf5},
        {0xe5, 0xf5, 0xe0},
        {0xc7, 0xe9, 0xc0},
        {0xa1, 0xd9, 0x9b},
        {0x74, 0xc4, 0x76},
        {0x41, 0xab, 0x5d},
        {0x23, 0x8b, 0x45},
        {0x00, 0x6d, 0x2c},
        {0x00, 0x44, 0x1b},
    };

    switch (cmap) {
        case ColorMapType::BREWER_RDBU: {
            return BREWER_RDBU;
        } break;
        case ColorMapType::BREWER_YLORBR: {
            return BREWER_YLORBR;
        } break;
        case ColorMapType::BREWER_BLUES: {
            return BREWER_BLUES;
        } break;
        case ColorMapType::BREWER_REDS: {
            return BREWER_REDS;
        } break;
        case ColorMapType::BREWER_GREENS: {
            return BREWER_GREENS;
        } break;
    };

    throw std::invalid_argument("unknown color map type");
    return BREWER_YLORBR;
}

std::array<uint8_t, 3> rgms::util::ColorMapColor(ColorMapType cmap, double v) {
    const std::vector<std::array<uint8_t, 3>>& cs = GetColorMapColors(cmap);
    if (cs.size() < 2) {
        throw std::invalid_argument("invalid color map");
    }

    if (v <= 0.0) {
        return cs.front();
    }
    if (v >= 1.0) {
        return cs.back();
    }

    std::vector<double> ls(cs.size());
    InplaceLinspaceBase(ls.begin(), ls.end(), 0.0, 1.0);
    auto i = std::lower_bound(ls.begin(), ls.end(), v);

    size_t li = std::distance(ls.begin(), i);

    double q = Lerp(v, ls[li-1], ls[li], 0.0, 1.0);

    std::array<uint8_t, 3> result;
    for (int j = 0; j < 3; j++) {
        result[j] = static_cast<uint8_t>(std::round(Lerp2(q,
                        static_cast<double>(cs[li-1][j]), static_cast<double>(cs[li][j]))));
    }

    return result;
}
