#include "../../include/memory/new.hpp"

void operator delete(void*) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete(void*, unsigned long) noexcept {}
void operator delete[](void*, unsigned long) noexcept {}
void operator delete(void*, unsigned long long) noexcept {}
void operator delete[](void*, unsigned long long) noexcept {}

extern "C" {
    void* __dso_handle = nullptr;
    
    int __cxa_atexit(void (*)(void*), void*, void*) {
        return 0;
    }
    
    void __cxa_pure_virtual() {
        while (1) {}
    }
    
    int atexit(void (*)(void)) {
        return 0;
    }
}
