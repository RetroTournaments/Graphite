////////////////////////////////////////////////////////////////////////////////
//
// General Utilities
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
#ifndef GRAPHITE_UTIL_HEADER
#define GRAPHITE_UTIL_HEADER

#include <chrono>

namespace graphite::util {

////////////////////////////////////////////////////////////////////////////////
// Arguments
////////////////////////////////////////////////////////////////////////////////
bool ArgPeekString(int* argc, char*** argv, std::string* str);
void ArgNext(int* argc, char*** argv);
bool ArgReadString(int* argc, char*** argv, std::string* str);
bool ArgReadInt(int* argc, char*** argv, int* v);
bool ArgReadInt64(int* argc, char*** argv, int64_t* v);
bool ArgReadDouble(int* argc, char*** argv, double* v);


////////////////////////////////////////////////////////////////////////////////
// Time
////////////////////////////////////////////////////////////////////////////////
using mclock = std::chrono::steady_clock;

mclock::time_point Now();
int64_t ToMillis(const mclock::duration& duration);
mclock::duration ToDuration(int64_t millis);
int64_t ElapsedMillis(const mclock::time_point& from, const mclock::time_point& to);
int64_t ElapsedMillisFrom(const mclock::time_point& from);

// Note simple in this context means: So simple as to be almost unusable
enum SimpleTimeFormatFlags : uint8_t {
    D    = 0b000001,
    H    = 0b000010,
    M    = 0b000100,
    S    = 0b001000,
    MS   = 0b010000,
    CS   = 0b100000,
    MSCS = 0b101100,
    HMS  = 0b001110,
};
std::string SimpleDurationFormat(const mclock::duration& duration, SimpleTimeFormatFlags flags);
std::string SimpleMillisFormat(int64_t millis, SimpleTimeFormatFlags flags);

}

#endif
