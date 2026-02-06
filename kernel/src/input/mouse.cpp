#include "../../include/input/mouse.hpp"
#include "../../include/cpu/io.hpp"
#include "../../include/graphics/framebuffer.hpp"

using namespace sertos::cpu;

namespace sertos::input {

MouseEvent Mouse::sBuffer[MOUSE_BUFFER_SIZE];
usize Mouse::sHead = 0;
usize Mouse::sTail = 0;

MouseState Mouse::sState = {0, 0, MouseButton::None};
MouseButton Mouse::sPrevButtons = MouseButton::None;

i32 Mouse::sMinX = 0;
i32 Mouse::sMinY = 0;
i32 Mouse::sMaxX = 1920;
i32 Mouse::sMaxY = 1080;

u8 Mouse::sPacket[4] = {0, 0, 0, 0};
u8 Mouse::sPacketIndex = 0;
bool Mouse::sHasScrollWheel = false;

bool Mouse::sInitialized = false;

namespace {

constexpr u8 PS2_DATA_PORT = 0x60;
constexpr u8 PS2_STATUS_PORT = 0x64;
constexpr u8 PS2_COMMAND_PORT = 0x64;

constexpr u8 PS2_STATUS_OUTPUT_FULL = 0x01;
constexpr u8 PS2_STATUS_INPUT_FULL = 0x02;
constexpr u8 PS2_STATUS_MOUSE_DATA = 0x20;

constexpr u8 PS2_CMD_READ_CONFIG = 0x20;
constexpr u8 PS2_CMD_WRITE_CONFIG = 0xD4;
constexpr u8 PS2_CMD_ENABLE_AUX = 0xA8;
constexpr u8 PS2_CMD_DISABLE_AUX = 0xA7;
constexpr u8 PS2_CMD_WRITE_AUX = 0xD4;

constexpr u8 MOUSE_CMD_RESET = 0xFF;
constexpr u8 MOUSE_CMD_ENABLE = 0xF4;
constexpr u8 MOUSE_CMD_DISABLE = 0xF5;
constexpr u8 MOUSE_CMD_SET_DEFAULTS = 0xF6;
constexpr u8 MOUSE_CMD_SET_SAMPLE_RATE = 0xF3;
constexpr u8 MOUSE_CMD_GET_DEVICE_ID = 0xF2;
constexpr u8 MOUSE_CMD_SET_RESOLUTION = 0xE8;

constexpr u8 MOUSE_ACK = 0xFA;
constexpr u8 MOUSE_RESEND = 0xFE;

constexpr u32 PS2_TIMEOUT = 100000;

}

void Mouse::waitWrite() {
    u32 timeout = PS2_TIMEOUT;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) && timeout > 0) {
        timeout--;
    }
}

void Mouse::waitRead() {
    u32 timeout = PS2_TIMEOUT;
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && timeout > 0) {
        timeout--;
    }
}

void Mouse::sendCommand(u8 cmd) {
    waitWrite();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_AUX);
    waitWrite();
    outb(PS2_DATA_PORT, cmd);
}

void Mouse::sendData(u8 data) {
    waitWrite();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_AUX);
    waitWrite();
    outb(PS2_DATA_PORT, data);
}

u8 Mouse::readData() {
    waitRead();
    return inb(PS2_DATA_PORT);
}

void Mouse::initialize() {
    sHead = 0;
    sTail = 0;
    sPacketIndex = 0;
    sState = {0, 0, MouseButton::None};
    sPrevButtons = MouseButton::None;
    
    sMaxX = static_cast<i32>(graphics::Framebuffer::width()) - 1;
    sMaxY = static_cast<i32>(graphics::Framebuffer::height()) - 1;
    sState.x = sMaxX / 2;
    sState.y = sMaxY / 2;
    
    waitWrite();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_AUX);
    
    waitWrite();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG);
    waitRead();
    u8 config = inb(PS2_DATA_PORT);
    
    config |= 0x02;
    config &= ~0x20;
    
    waitWrite();
    outb(PS2_COMMAND_PORT, 0x60);
    waitWrite();
    outb(PS2_DATA_PORT, config);
    
    sendCommand(MOUSE_CMD_SET_DEFAULTS);
    readData();
    
    sendCommand(MOUSE_CMD_SET_SAMPLE_RATE);
    readData();
    sendData(200);
    readData();
    
    sendCommand(MOUSE_CMD_SET_SAMPLE_RATE);
    readData();
    sendData(100);
    readData();
    
    sendCommand(MOUSE_CMD_SET_SAMPLE_RATE);
    readData();
    sendData(80);
    readData();
    
    sendCommand(MOUSE_CMD_GET_DEVICE_ID);
    readData();
    u8 deviceId = readData();
    
    sHasScrollWheel = (deviceId == 3 || deviceId == 4);
    
    sendCommand(MOUSE_CMD_SET_SAMPLE_RATE);
    readData();
    sendData(100);
    readData();
    
    sendCommand(MOUSE_CMD_SET_RESOLUTION);
    readData();
    sendData(3);
    readData();
    
    sendCommand(MOUSE_CMD_ENABLE);
    readData();
    
    sInitialized = true;
}

void Mouse::handlePacket(u8 byte) {
    if (!sInitialized) return;
    
    if (sPacketIndex == 0 && !(byte & 0x08)) {
        return;
    }
    
    sPacket[sPacketIndex++] = byte;
    
    u8 packetSize = sHasScrollWheel ? 4 : 3;
    
    if (sPacketIndex >= packetSize) {
        sPacketIndex = 0;
        
        MouseEvent event;
        event.deltaX = sPacket[1];
        event.deltaY = sPacket[2];
        event.deltaScroll = 0;
        
        if (sPacket[0] & 0x10) {
            event.deltaX |= 0xFFFFFF00;
        }
        if (sPacket[0] & 0x20) {
            event.deltaY |= 0xFFFFFF00;
        }
        
        event.deltaY = -event.deltaY;
        
        if (sHasScrollWheel && packetSize == 4) {
            i8 scroll = static_cast<i8>(sPacket[3]);
            event.deltaScroll = scroll;
        }
        
        MouseButton newButtons = MouseButton::None;
        if (sPacket[0] & 0x01) newButtons = newButtons | MouseButton::Left;
        if (sPacket[0] & 0x02) newButtons = newButtons | MouseButton::Right;
        if (sPacket[0] & 0x04) newButtons = newButtons | MouseButton::Middle;
        
        event.buttons = newButtons;
        
        event.pressed = static_cast<MouseButton>(
            static_cast<u8>(newButtons) & ~static_cast<u8>(sPrevButtons));
        event.released = static_cast<MouseButton>(
            static_cast<u8>(sPrevButtons) & ~static_cast<u8>(newButtons));
        
        sPrevButtons = newButtons;
        
        sState.x += event.deltaX;
        sState.y += event.deltaY;
        
        if (sState.x < sMinX) sState.x = sMinX;
        if (sState.x > sMaxX) sState.x = sMaxX;
        if (sState.y < sMinY) sState.y = sMinY;
        if (sState.y > sMaxY) sState.y = sMaxY;
        
        sState.buttons = newButtons;
        
        pushEvent(event);
    }
}

void Mouse::poll() {
    if (!sInitialized) return;
    
    u8 status = inb(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_FULL) && (status & PS2_STATUS_MOUSE_DATA)) {
        u8 data = inb(PS2_DATA_PORT);
        handlePacket(data);
    }
}

bool Mouse::hasEvent() {
    return sHead != sTail;
}

MouseEvent Mouse::getEvent() {
    if (!hasEvent()) {
        return {0, 0, 0, MouseButton::None, MouseButton::None, MouseButton::None};
    }
    
    MouseEvent event = sBuffer[sTail];
    sTail = (sTail + 1) % MOUSE_BUFFER_SIZE;
    return event;
}

void Mouse::setPosition(i32 x, i32 y) {
    sState.x = x;
    sState.y = y;
    
    if (sState.x < sMinX) sState.x = sMinX;
    if (sState.x > sMaxX) sState.x = sMaxX;
    if (sState.y < sMinY) sState.y = sMinY;
    if (sState.y > sMaxY) sState.y = sMaxY;
}

void Mouse::setBounds(i32 minX, i32 minY, i32 maxX, i32 maxY) {
    sMinX = minX;
    sMinY = minY;
    sMaxX = maxX;
    sMaxY = maxY;
    
    if (sState.x < sMinX) sState.x = sMinX;
    if (sState.x > sMaxX) sState.x = sMaxX;
    if (sState.y < sMinY) sState.y = sMinY;
    if (sState.y > sMaxY) sState.y = sMaxY;
}

void Mouse::pushEvent(const MouseEvent& event) {
    usize nextHead = (sHead + 1) % MOUSE_BUFFER_SIZE;
    if (nextHead == sTail) {
        return;
    }
    
    sBuffer[sHead] = event;
    sHead = nextHead;
}

}
