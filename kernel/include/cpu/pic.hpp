#pragma once

#include "../types.hpp"

namespace sertos::cpu {

constexpr u16 PIC1_COMMAND = 0x20;
constexpr u16 PIC1_DATA = 0x21;
constexpr u16 PIC2_COMMAND = 0xA0;
constexpr u16 PIC2_DATA = 0xA1;

constexpr u8 PIC_EOI = 0x20;

constexpr u8 ICW1_ICW4 = 0x01;
constexpr u8 ICW1_SINGLE = 0x02;
constexpr u8 ICW1_INTERVAL4 = 0x04;
constexpr u8 ICW1_LEVEL = 0x08;
constexpr u8 ICW1_INIT = 0x10;

constexpr u8 ICW4_8086 = 0x01;
constexpr u8 ICW4_AUTO = 0x02;
constexpr u8 ICW4_BUF_SLAVE = 0x08;
constexpr u8 ICW4_BUF_MASTER = 0x0C;
constexpr u8 ICW4_SFNM = 0x10;

class PIC {
public:
    static void initialize();
    static void disable();
    static void sendEOI(u8 irq);
    static void setMask(u8 irq);
    static void clearMask(u8 irq);
    static u16 getIRR();
    static u16 getISR();

private:
    static void remap(u8 offset1, u8 offset2);
    static u8 readRegister(u8 ocw3);
};

}
