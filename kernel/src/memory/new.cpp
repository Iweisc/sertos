#include "../../include/memory/new.hpp"
#include "../../include/memory/pmm.hpp"

void* operator new(unsigned long long size) {
    return sertos::memory::PMM::allocatePages((size + 4095) / 4096);
}

void* operator new[](unsigned long long size) {
    return sertos::memory::PMM::allocatePages((size + 4095) / 4096);
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        sertos::memory::PMM::freePages(ptr, 1);
    }
}

void operator delete[](void* ptr) noexcept {
    if (ptr) {
        sertos::memory::PMM::freePages(ptr, 1);
    }
}

void operator delete(void* ptr, unsigned long) noexcept {
    if (ptr) {
        sertos::memory::PMM::freePages(ptr, 1);
    }
}

void operator delete[](void* ptr, unsigned long) noexcept {
    if (ptr) {
        sertos::memory::PMM::freePages(ptr, 1);
    }
}

void operator delete(void* ptr, unsigned long long) noexcept {
    if (ptr) {
        sertos::memory::PMM::freePages(ptr, 1);
    }
}

void operator delete[](void* ptr, unsigned long long) noexcept {
    if (ptr) {
        sertos::memory::PMM::freePages(ptr, 1);
    }
}

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
