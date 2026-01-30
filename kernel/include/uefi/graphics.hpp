#pragma once

#include "types.hpp"

namespace sertos::uefi {

enum class EfiGraphicsPixelFormat : u32 {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
};

struct EfiPixelBitmask {
    u32 redMask;
    u32 greenMask;
    u32 blueMask;
    u32 reservedMask;
};

struct EfiGraphicsOutputModeInformation {
    u32 version;
    u32 horizontalResolution;
    u32 verticalResolution;
    EfiGraphicsPixelFormat pixelFormat;
    EfiPixelBitmask pixelInformation;
    u32 pixelsPerScanLine;
};

struct EfiGraphicsOutputProtocolMode {
    u32 maxMode;
    u32 mode;
    EfiGraphicsOutputModeInformation* info;
    usize sizeOfInfo;
    EfiPhysicalAddress frameBufferBase;
    usize frameBufferSize;
};

struct EfiGraphicsOutputBltPixel {
    u8 blue;
    u8 green;
    u8 red;
    u8 reserved;
};

enum class EfiGraphicsOutputBltOperation : u32 {
    BltVideoFill,
    BltVideoToBltBuffer,
    BltBufferToVideo,
    BltVideoToVideo,
    GraphicsOutputBltOperationMax
};

struct EfiGraphicsOutputProtocol {
    EfiStatus (*queryMode)(EfiGraphicsOutputProtocol* self, u32 modeNumber, usize* sizeOfInfo, EfiGraphicsOutputModeInformation** info);
    EfiStatus (*setMode)(EfiGraphicsOutputProtocol* self, u32 modeNumber);
    EfiStatus (*blt)(EfiGraphicsOutputProtocol* self, EfiGraphicsOutputBltPixel* bltBuffer, EfiGraphicsOutputBltOperation bltOperation, usize sourceX, usize sourceY, usize destinationX, usize destinationY, usize width, usize height, usize delta);
    EfiGraphicsOutputProtocolMode* mode;
};

}
