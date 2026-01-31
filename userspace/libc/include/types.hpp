#pragma once

namespace sertos::libc {

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

using pid_t = i32;
using ssize_t = i64;

constexpr u64 PAGE_SIZE = 4096;

}
