#pragma once

#include "types.hpp"
#include "../../boot/include/bootinfo.hpp"

namespace sertos {

class Kernel {
public:
    static void initialize(boot::BootInfo* bootInfo);
    static boot::BootInfo* bootInfo();
    static void panic(const char* message);
    static void halt();

private:
    static boot::BootInfo* sBootInfo;
};

}
