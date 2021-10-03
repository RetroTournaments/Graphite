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
#ifndef RGMS_RGMUTIL_HEADER
#define RGMS_RGMUTIL_HEADER

#include <chrono>
#include <type_traits>
#include <algorithm>
#include <iterator>
#include <random>
#include <iostream>
#include <string>
#include <array>
#include <cmath>
#include <functional>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace rgms::util {

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

////////////////////////////////////////////////////////////////////////////////
// Bitmanip
////////////////////////////////////////////////////////////////////////////////
inline uint8_t Reverse(uint8_t b) {
    // https://stackoverflow.com/a/2602885
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
template <typename T> 
bool Clamp(T* v, T lo, T hi) {
    assert(v);
    bool r = false;
    if (*v < lo) {
        *v = lo;
        r = true;
    }
    if (*v > hi) {
        *v = hi;
        r = true;
    }
    return r;
}

template <typename T> 
T Clamped(T v, T lo, T hi) {
    Clamp(&v, lo, hi);
    return v;
}

////////////////////////////////////////////////////////////////////////////////
// Linspace
//
// std::vector<float> arr(100);
// InplaceLinspace(arr.begin(), arr.end(), 0, 14);
// 
// for (auto v : linspace(0, 1, 100) {
// }
////////////////////////////////////////////////////////////////////////////////
template <class Iter, typename T>
void InplaceLinspaceBase(Iter begin, Iter end, T start, T stop) {
    typedef typename std::iterator_traits<Iter>::difference_type diff_t;
    typedef typename std::make_unsigned<diff_t>::type udiff_t;

    udiff_t n = end - begin;

    T delta = stop - start;
    T step = delta / static_cast<T>(n - 1);

    udiff_t i = 0;
    for (auto it = begin; it != end; ++it) {
        *it = start + i * step;
        i++;
    }
}

template <class Iter>
void InplaceLinspace(Iter begin, Iter end, float start, float stop) {
    InplaceLinspaceBase<Iter, float>(begin, end, start, stop);
}

// The linspace generator / iterator allows for the nice:
//
// for (auto v : linspace(0, 1, 100)) {
//     std::cout << v << std::endl;
// }
//
// without storing the entire thing in memory
template <typename T>
struct LinspaceIterator {
    LinspaceIterator(T start, T step, int i) 
        : m_start(start)
        , m_step(step)
        , m_i(i)
    {}
    ~LinspaceIterator() {}

    bool operator!= (const LinspaceIterator& other) const {
        return m_i != other.m_i;
    }

    T operator* () const {
        return m_start + m_i * m_step;
    }

    const LinspaceIterator& operator++ () {
        ++m_i;
        return *this;
    }

    T m_start, m_step;
    int m_i;
};

// Generator for linspace
template <typename T>
struct LinspaceGeneratorBase {
    LinspaceGeneratorBase(T start, T stop, int n)
        : m_start(start)
        , m_n(n) {
        T delta = stop - start;
        m_step = delta / static_cast<T>(n - 1);
    }
    ~LinspaceGeneratorBase() {};

    LinspaceIterator<T> begin() const {
        return LinspaceIterator<T>(m_start, m_step, 0);
    }
    LinspaceIterator<T> end() const {
        return LinspaceIterator<T>(m_start, m_step, m_n);
    }

    T m_start, m_step;
    int m_n;
};
typedef LinspaceGeneratorBase<float> Linspace;


////////////////////////////////////////////////////////////////////////////////
// Geometry
////////////////////////////////////////////////////////////////////////////////
// The main base vector class
template <typename T, size_t n>
struct VectorBase {
    //static_assert(n >= 1, "Error: You many not initialize a vector with less than two dimensions");
    VectorBase() {};
    ~VectorBase() {};

    T& operator[](size_t idx) { return data[idx]; };
    const T& operator[](size_t idx) const { return data[idx]; };

    VectorBase& operator+=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] += rhs[i];
        return *this;
    }
    VectorBase& operator-=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] -= rhs[i];
        return *this;
    }
    VectorBase& operator*=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] *= rhs[i];
        return *this;
    }
    VectorBase& operator/=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] /= rhs[i];
        return *this;
    }
    VectorBase& operator+=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] += rhs;
        return *this;
    }
    VectorBase& operator-=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] -= rhs;
        return *this;
    }
    VectorBase& operator*=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] *= rhs;
        return *this;
    }
    VectorBase& operator/=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] /= rhs;
        return *this;
    }
    VectorBase operator-() const {
        auto copy = *this;
        for (size_t i = 0; i < n; i++)
            copy[i] = -copy[i];
        return copy;
    }

    static VectorBase Origin() {
        VectorBase o;
        std::fill(o.data.begin(), o.data.end(), T(0));
        return o;
    }
    //constexpr static size_t Dimensions() {
    //    return n;
    //}

    std::array<T, n> data;
};
// Binary operations with another vector
template <typename T, size_t n>
inline VectorBase<T, n> operator+(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs += rhs; return lhs;
}
template <typename T, size_t n>
inline VectorBase<T, n> operator-(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs -= rhs; return lhs;
}
template <typename T, size_t n>
inline VectorBase<T, n> operator*(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs *= rhs; return lhs;
}
template <typename T, size_t n>
inline VectorBase<T, n> operator/(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs /= rhs; return lhs;
}
// Binary operations with a scalar
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator+(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs += static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator-(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs -= static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator*(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs *= static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator/(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs /= static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator*(U lhs, VectorBase<T, n> rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    rhs *= lhs; return rhs;
}
// Equality operators
template <typename T, size_t n>
inline bool operator==(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return std::equal(lhs.data.begin(), lhs.data.end(), rhs.data.begin());
}
template <typename T, size_t n>
inline bool operator!=(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return !operator==(lhs, rhs);
}
// Lexicographic comparisons
template <typename T, size_t n>
inline bool operator< (const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    for (size_t i = 0; i < n - 1; i++)
        if (lhs[i] != rhs[i])
            return lhs[i] < rhs[i];
    return lhs[n - 1] < rhs[n - 1];
}
template <typename T, size_t n>
inline bool operator> (const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return operator<(rhs, lhs);
}
template <typename T, size_t n>
inline bool operator<=(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return !operator>(lhs, rhs);
}
template <typename T, size_t n>
inline bool operator>=(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return !operator<(lhs, rhs);
}
// General Operations
template <typename T, size_t n>
inline T Distance(const VectorBase<T, n>& a, const VectorBase<T, n>& b) {
    return Magnitude(b - a);
}
template <typename T, size_t n>
inline T DistanceSquared(const VectorBase<T, n>& a, const VectorBase<T, n>& b) {
    return MagnitudeSquared(b - a);
}
template <typename T, size_t n>
inline T Dot(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    auto f1 = lhs.data.begin(); auto l1 = lhs.data.end();
    auto f2 = rhs.data.begin();
    T v = T(0);
    while (f1 != l1) {
        v += *f1 * *f2;
        ++f1; ++f2;
    }
    return v;
}
template <typename T, size_t n>
inline T MagnitudeSquared(const VectorBase<T, n>& vec) {
    return Dot(vec, vec);
}
template <typename T, size_t n>
inline T Magnitude(const VectorBase<T, n>& vec) {
    //static_assert(!std::is_integral<T>(), "Magnitude is invalid for integral types");
    return std::sqrt(MagnitudeSquared(vec));
}
template <typename T, size_t n>
inline T Theta(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    //static_assert(!std::is_integral<T>(), "Theta is invalid for integral types");
    return std::acos(Dot(lhs, rhs) / (Magnitude(lhs) * Magnitude(rhs)));
}
template <typename T, size_t n>
inline VectorBase<T, n>& Normalize(VectorBase<T, n>& vec) {
    //static_assert(!std::is_integral<T>(), "Normalize is invalid for integral types");
    vec /= Magnitude(vec);
    return vec;
}
template <typename T, size_t n>
inline VectorBase<T, n>* Normalize(VectorBase<T, n>* vec) {
   (*vec) /= Magnitude(*vec);
   return vec;
}
template <typename T, size_t n>
inline VectorBase<T, n> Normalized(VectorBase<T, n> vec) {
    return Normalize(vec);
}
// 2D operations, higher dimensions are ignored
template <typename T, size_t n>
inline double TriArea2(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return ((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]));
}
template <typename T, size_t n>
inline double TriArea(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    //static_assert(!std::is_integral<T>(), "TriArea is invalid for integral types");
    return TriArea2(a, b, c) / 2.0;
}
template <typename T, size_t n>
inline bool IsLeft(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return TriArea2(a, b, c) > 0.0;
}
template <typename T, size_t n>
inline bool IsRight(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return TriArea2(a, b, c) < 0.0;
}
template <typename T, size_t n>
inline bool IsCollinear(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return TriArea2(a, b, c) == 0.0;
}

// Output
template <typename T, size_t n>
inline std::ostream& operator<<(std::ostream& os, const VectorBase<T, n>& vec) {
    os << "{";
    for (size_t i = 0; i < n - 1; i++)
        os << vec[i] << ", ";
    os << vec[n - 1] << "}";
    return os;
}
template <typename T, size_t n>
inline std::istream& operator>>(std::istream& is, VectorBase<T, n>& vec) {
    std::string str;
    std::getline(is, str, '{');
    for (size_t i = 0; i < n - 1; i++) {
        std::getline(is, str, ',');
        //vec[i] = std::stod(str);
        vec[i] = ::atof(str.c_str());
    }
    std::getline(is, str, '}');
    //vec[n - 1] = std::stod(str);
    vec[n - 1] = ::atof(str.c_str());
    return is;
}

// Specializations for the most common dimensions
template <typename T>
struct VectorBase<T, 3> {
    VectorBase() {};
    VectorBase(T x, T y, T z) : x(x), y(y), z(z) {};

    T& operator[](size_t idx) { return data[idx]; };
    const T& operator[](size_t idx) const { return data[idx]; };

    VectorBase& operator+=(const VectorBase& rhs) {
        x += rhs.x; y += rhs.y; z += rhs.z;
        return *this;
    }
    VectorBase& operator-=(const VectorBase& rhs) {
        x -= rhs.x; y -= rhs.y; z -= rhs.z;
        return *this;
    }
    VectorBase& operator*=(const VectorBase& rhs) {
        x *= rhs.x; y *= rhs.y; z *= rhs.z;
        return *this;
    }
    VectorBase& operator/=(const VectorBase& rhs) {
        x /= rhs.x; y /= rhs.y; z /= rhs.z;
        return *this;
    }
    VectorBase& operator+=(T rhs) {
        x += rhs; y += rhs; z += rhs;
        return *this;
    }
    VectorBase& operator-=(T rhs) {
        x -= rhs; y -= rhs; z -= rhs;
        return *this;
    }
    VectorBase& operator*=(T rhs) {
        x *= rhs; y *= rhs; z *= rhs;
        return *this;
    }
    VectorBase& operator/=(T rhs) {
        x /= rhs; y /= rhs; z /= rhs;
        return *this;
    }
    VectorBase operator-() const {
        return VectorBase(-x, -y, -z);
    }

    static VectorBase Origin() {
        return VectorBase(0.0, 0.0, 0.0);
    }
    static VectorBase XAxis() {
        return VectorBase(1.0, 0.0, 0.0);
    }
    static VectorBase YAxis() {
        return VectorBase(0.0, 1.0, 0.0);
    }
    static VectorBase ZAxis() {
        return VectorBase(0.0, 0.0, 1.0);
    }

    //constexpr static size_t Dimensions() {
    //    return 3;
    //}


    union {
        std::array<T, 3> data;
        struct { T x, y, z;};
        //VectorBase<T, 2> xy;
    };
};
template <typename T>
VectorBase<T, 3> inline Cross(const VectorBase<T, 3>& lhs, const VectorBase<T, 3>& rhs) {
    return VectorBase<T, 3>(lhs.y*rhs.z - lhs.z*rhs.y, lhs.z*rhs.x - lhs.x*rhs.z, lhs.x*rhs.y - lhs.y*rhs.x);
}

template <typename T>
struct VectorBase<T, 2> {
    VectorBase() {};
    VectorBase(VectorBase<T, 3> vec) : x(vec.x), y(vec.y) {};
    VectorBase(T x, T y) : x(x), y(y) {};

    T& operator[](size_t idx) { return data[idx]; };
    const T& operator[](size_t idx) const { return data[idx]; };

    VectorBase& operator+=(const VectorBase& rhs) {
        x += rhs.x; y += rhs.y;
        return *this;
    }
    VectorBase& operator-=(const VectorBase& rhs) {
        x -= rhs.x; y -= rhs.y;
        return *this;
    }
    VectorBase& operator*=(const VectorBase& rhs) {
        x *= rhs.x; y *= rhs.y;
        return *this;
    }
    VectorBase& operator/=(const VectorBase& rhs) {
        x /= rhs.x; y /= rhs.y;
        return *this;
    }
    VectorBase& operator+=(T rhs) {
        x += rhs; y += rhs;
        return *this;
    }
    VectorBase& operator-=(T rhs) {
        x -= rhs; y -= rhs;
        return *this;
    }
    VectorBase& operator*=(T rhs) {
        x *= rhs; y *= rhs;
        return *this;
    }
    VectorBase& operator/=(T rhs) {
        x /= rhs; y /= rhs;
        return *this;
    }
    VectorBase operator-() const {
        return VectorBase(-x, -y);
    }

    static VectorBase Origin() {
        return VectorBase(0.0, 0.0);
    }
    static VectorBase XAxis() {
        return VectorBase(1.0, 0.0);
    }
    static VectorBase YAxis() {
        return VectorBase(0.0, 1.0);
    }

    //constexpr static size_t Dimensions() {
    //    return 2;
    //}

    union {
        std::array<T, 2> data;
        struct { T x, y; };
    };
};

typedef VectorBase<double, 3> Vector3D;
typedef VectorBase<double, 2> Vector2D;
typedef VectorBase<float, 3> Vector3F;
typedef VectorBase<float, 2> Vector2F;
typedef VectorBase<long, 3> Vector3L;
typedef VectorBase<long, 2> Vector2L;
typedef VectorBase<int, 3> Vector3I;
typedef VectorBase<int, 2> Vector2I;


/*  0   1   2   3
 *  4   5   6   7
 *  8   9  10  11
 * 12  13  14  15
 */
typedef std::array<Vector2F, 16> BezierPatch;
BezierPatch RectanglePatch(Vector2F origin, Vector2F size);
Vector2F EvaluateBezier(float t,
                        const Vector2F& a, 
                        const Vector2F& b, 
                        const Vector2F& c,
                        const Vector2F& d);

// x, y, width, height
template <typename T>
struct RectBase {
    RectBase() {};
    RectBase(T x, T y, T w, T h)
        : X(x)
        , Y(y)
        , Width(w)
        , Height(h)
    {
    }
    ~RectBase() {};

    T X, Y;
    T Width, Height;

    util::VectorBase<T, 2> TopLeft() const {
        return util::VectorBase<T, 2>(X, Y);
    }
    util::VectorBase<T, 2> BottomRight() const {
        return util::VectorBase<T, 2>(X + Width, Y + Height);
    }
};

typedef RectBase<int> Rect2I;
typedef RectBase<float> Rect2F;

typedef std::array<Vector2F, 4> Quadrilateral2F;

////////////////////////////////////////////////////////////////////////////////
// JSON
////////////////////////////////////////////////////////////////////////////////

// For a basic struct it is:
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(typename, member1, member2...);

#define JSONEXT_FROM_TO_CLASS(cname, fromfunc, tofunc)              \
inline void to_json(nlohmann::json& j, const cname& c) {            \
    j = c.tofunc();                                                 \
}                                                                   \
inline void from_json(const nlohmann::json& j, cname& c) {          \
    c.fromfunc(j);                                                  \
}

// Example:
//   enum class Friends {
//    EDGAR,
//    ARNIE,
//   };
//   NLOHMANN_JSON_SERIALIZE_ENUM(Friends, {
//    {Friends::EDGAR, "edgar"},
//    {Friends::ARNIE, "arnie"},
//   })
//   JSONEXT_SERIALIZE_ENUM_OPERATORS(Friends);
//
//  TODO add 'AllFriends' maybe?
#define JSONEXT_SERIALIZE_ENUM_OPERATORS(etype)                     \
inline std::ostream& operator<<(std::ostream& os, const etype& e) { \
    nlohmann::json j = e;                                           \
    os << std::string(j);                                           \
    return os;                                                      \
}                                                                   \
inline std::istream& operator>>(std::istream& is, etype& e) {       \
    std::string v;                                                  \
    is >> v;                                                        \
    nlohmann::json j = v;                                           \
    e = j.get<etype>();                                             \
    return is;                                                      \
}

////////////////////////////////////////////////////////////////////////////////
// File system
////////////////////////////////////////////////////////////////////////////////
namespace fs = std::experimental::filesystem;

void ForFileInDirectory(const std::string& directory,
        std::function<bool(fs::path p)> cback);
// Include "." so: ".png"
void ForFileOfExtensionInDirectory(const std::string& directory,
        const std::string& extension,
        std::function<bool(fs::path p)> cback);

int ReadFileToVector(const std::string& path, std::vector<uint8_t>* contents);
void WriteVectorToFile(const std::string& path, const std::vector<uint8_t>& contents);


////////////////////////////////////////////////////////////////////////////////
// Strings
////////////////////////////////////////////////////////////////////////////////
bool StringEndsWith(const std::string& str, const std::string& ending);
bool StringStartsWith(const std::string& str, const std::string& start);

////////////////////////////////////////////////////////////////////////////////
// Reservoir Sampling
////////////////////////////////////////////////////////////////////////////////
template <class RandomIt, class UniformRandomNumberGenerator>
void ReservoirSampleVec(const RandomIt first, const RandomIt last, RandomIt outputFirst, RandomIt outputLast,
                      UniformRandomNumberGenerator&& g) {
    typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
    typedef typename std::make_unsigned<diff_t>::type udiff_t;
    typedef typename std::uniform_int_distribution<udiff_t> distr_t;

    std::uniform_real_distribution<double> real_dist(std::nextafter(0, 10), std::nextafter(1, 10));

    udiff_t n = last - first;
    udiff_t R = outputLast - outputFirst;

    // fill the output with the first R elements
    udiff_t j = 0;
    while (j < R && j < n) {
        *std::next(outputFirst, j) = *std::next(first, j);
        j++;
    }

    // do regular reservoir sampling up to a certain point
    udiff_t t = R * 4;
    if (t < 200) {
        t = 200;
    }
    while (j <= t && j < n) {
        udiff_t k = distr_t(0, j)(g);
        if (k < R) {
            *std::next(outputFirst, k) = *std::next(first, j);
        }
        j++;
    }

    // do gap sampling
    while (j < n) {
        double p = static_cast<double>(R) / static_cast<double>(j);
        double u = real_dist(g);
        udiff_t gap = static_cast<udiff_t>(std::log(u) / std::log1p(-p));

        j += gap;
        if (j < n) {
            udiff_t k = distr_t(0, R - 1)(g);
            *std::next(outputFirst, k) = *std::next(first, j);
        }
        j++;
    }
}

template <typename T>
class OnlineReservoirSampler {
public:
    OnlineReservoirSampler(size_t given_reservoir_size, int seed) 
        : Reservoir(given_reservoir_size)
        , m_ReservoirSize(given_reservoir_size) 
        , m_Count(0)
        , m_GapCutoff(given_reservoir_size * 4)
        , m_Gap(0)
        , m_Engine(seed)
    {
    };
    ~OnlineReservoirSampler() {};

    void Process(const T& val) {
        if (m_Gap > 0) {
            m_Gap--;
        } else {
            if (m_Count < m_ReservoirSize) {
                Reservoir[m_Count] = val;
            } else if (m_Count < m_GapCutoff) {
                size_t k = GetK(m_Count);
                if (k < m_ReservoirSize) {
                    Reservoir[k] = val;
                }
            } else {
                if (m_Count == m_GapCutoff) {
                    m_Gap = GetGap();

                    if (m_Gap == 0) {
                        size_t k = GetK(m_ReservoirSize - 1);
                        Reservoir[k] = val;
                    }
                } else {
                    size_t k = GetK(m_ReservoirSize - 1);
                    Reservoir[k] = val;

                    m_Gap = GetGap();
                }
            }
        }

        m_Count++;
    }

    std::vector<T> Reservoir;
private:
    size_t m_ReservoirSize;
    size_t m_Count;
    size_t m_GapCutoff;
    size_t m_Gap;

    size_t GetK(size_t top) {
        typedef std::uniform_int_distribution<size_t> distr_t;
        return distr_t(0, top)(m_Engine);
    }

    size_t GetGap() {
        double p = static_cast<double>(m_ReservoirSize) / static_cast<double>(m_Count);
        double u = std::uniform_real_distribution<double>(std::nextafter(0, 10),
                std::nextafter(1, 10))(m_Engine);
        return std::floor(std::log(u) / std::log1p(-p));
    }

    std::mt19937_64 m_Engine;
};

}

#endif
