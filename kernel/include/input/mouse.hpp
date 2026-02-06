#pragma once

#include "../types.hpp"

namespace sertos::input {

constexpr usize MOUSE_BUFFER_SIZE = 64;

enum class MouseButton : u8 {
    None = 0,
    Left = 1 << 0,
    Right = 1 << 1,
    Middle = 1 << 2
};

inline MouseButton operator|(MouseButton a, MouseButton b) {
    return static_cast<MouseButton>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline MouseButton operator&(MouseButton a, MouseButton b) {
    return static_cast<MouseButton>(static_cast<u8>(a) & static_cast<u8>(b));
}

inline bool hasButton(MouseButton buttons, MouseButton button) {
    return (static_cast<u8>(buttons) & static_cast<u8>(button)) != 0;
}

struct MouseEvent {
    i32 deltaX;
    i32 deltaY;
    i32 deltaScroll;
    MouseButton buttons;
    MouseButton pressed;
    MouseButton released;
};

struct MouseState {
    i32 x;
    i32 y;
    MouseButton buttons;
};

class Mouse {
public:
    static void initialize();
    static bool isInitialized() { return sInitialized; }
    
    static void handlePacket(u8 byte);
    
    static bool hasEvent();
    static MouseEvent getEvent();
    
    static MouseState state() { return sState; }
    static i32 x() { return sState.x; }
    static i32 y() { return sState.y; }
    static MouseButton buttons() { return sState.buttons; }
    
    static bool isLeftPressed() { return hasButton(sState.buttons, MouseButton::Left); }
    static bool isRightPressed() { return hasButton(sState.buttons, MouseButton::Right); }
    static bool isMiddlePressed() { return hasButton(sState.buttons, MouseButton::Middle); }
    
    static void setPosition(i32 x, i32 y);
    static void setBounds(i32 minX, i32 minY, i32 maxX, i32 maxY);
    
    static void poll();

private:
    static void pushEvent(const MouseEvent& event);
    static void waitWrite();
    static void waitRead();
    static void sendCommand(u8 cmd);
    static void sendData(u8 data);
    static u8 readData();
    
    static MouseEvent sBuffer[MOUSE_BUFFER_SIZE];
    static usize sHead;
    static usize sTail;
    
    static MouseState sState;
    static MouseButton sPrevButtons;
    
    static i32 sMinX, sMinY, sMaxX, sMaxY;
    
    static u8 sPacket[4];
    static u8 sPacketIndex;
    static bool sHasScrollWheel;
    
    static bool sInitialized;
};

}
