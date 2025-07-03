#pragma once 

#include <stdint.h>

#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <algorithm>
#include <numeric>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

template<typename T, typename Allocator = std::allocator<T>>
using DynamicArray = std::vector<T, Allocator>;

template<typename T, size_t Size>
using StaticArray = std::array<T, Size>;

template<typename T, typename Allocator = std::allocator<T>>
using HashMap = std::unordered_map<T, Allocator>;

template<typename T, class Deleter = std::default_delete<T>>
using UniquePtr = std::unique_ptr<T, Deleter>;

template<typename T, typename... Args>
UniquePtr<T> make_unique_ptr(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
constexpr std::remove_reference_t<T>&& move_ptr(T&& arg) noexcept
{
    return std::move(arg);
}

using Mutex = std::mutex;
using ScopedLock = std::scoped_lock<std::mutex>;

template<typename Iterator>
inline void sort_container(Iterator first, Iterator last)
{
    std::sort(first, last);
}

template <typename Iterator, typename Compare>
inline void sort_container(Iterator first, Iterator last, Compare comp)
{
    std::sort(first, last, comp);
}

template <typename ForwardIt, typename T>
void fill_sequential(ForwardIt first, ForwardIt last, T value)
{
    std::iota(first, last, value);
}
