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
