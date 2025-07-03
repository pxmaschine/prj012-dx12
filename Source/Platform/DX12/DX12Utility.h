#pragma once

#include <cstdint>

template <class T> void safe_release_com_ptr(T& ptr_t)
{
    if (ptr_t)
    {
        ptr_t->Release();
        ptr_t = nullptr;
    }
}

template <typename T>
struct COMDeleter
{
    void operator()(T* ptr) const
    {
        safe_release_com_ptr(ptr);
    }
};

inline uint32_t align_u32(uint32_t value_to_align, uint32_t alignment)
{
    alignment -= 1;
    return (uint32_t)((value_to_align + alignment) & ~alignment);
}

inline uint64_t align_u64(uint64_t value_to_align, uint64_t alignment)
{
    alignment -= 1;
    return (uint64_t)((value_to_align + alignment) & ~alignment);
}
