#pragma once

using size_t = unsigned long long;

namespace sertos {

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8 = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using usize = u64;
using isize = i64;

using uptr = u64;
using iptr = i64;

constexpr u64 KB = 1024;
constexpr u64 MB = 1024 * KB;
constexpr u64 GB = 1024 * MB;

constexpr uptr nullptr_v = 0;

template<typename T>
constexpr T align_up(T value, T alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

template<typename T>
constexpr T align_down(T value, T alignment) {
    return value & ~(alignment - 1);
}

template<typename T>
constexpr bool is_aligned(T value, T alignment) {
    return (value & (alignment - 1)) == 0;
}

}
