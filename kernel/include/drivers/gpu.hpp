#pragma once

#include "../types.hpp"

namespace sertos::drivers {

constexpr u32 MAX_GPU_DEVICES = 4;
constexpr u32 MAX_GPU_CONTEXTS = 64;
constexpr u32 MAX_GPU_BUFFERS = 256;
constexpr u32 MAX_GPU_COMMAND_BUFFERS = 32;

enum class GpuVendor : u8 {
    Unknown = 0,
    Intel,
    AMD,
    Nvidia,
    VirtIO,
    VMware,
    Bochs
};

enum class GpuFeature : u32 {
    None = 0,
    Acceleration2D = 1 << 0,
    Acceleration3D = 1 << 1,
    VideoDecoding = 1 << 2,
    VideoEncoding = 1 << 3,
    Compute = 1 << 4,
    MultiMonitor = 1 << 5,
    VSync = 1 << 6,
    Cursor = 1 << 7
};

enum class GpuBufferType : u8 {
    Vertex = 0,
    Index,
    Uniform,
    Texture,
    RenderTarget,
    DepthStencil,
    Staging
};

enum class GpuTextureFormat : u8 {
    Unknown = 0,
    R8,
    RG8,
    RGB8,
    RGBA8,
    BGR8,
    BGRA8,
    R16F,
    RG16F,
    RGBA16F,
    R32F,
    RG32F,
    RGBA32F,
    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8
};

enum class GpuPrimitiveType : u8 {
    Points = 0,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan
};

enum class GpuBlendMode : u8 {
    None = 0,
    Alpha,
    Additive,
    Multiply
};

struct GpuDeviceInfo {
    char name[64];
    GpuVendor vendor;
    u32 vendorId;
    u32 deviceId;
    u64 videoMemory;
    u32 maxTextureSize;
    u32 maxRenderTargets;
    u32 features;
};

struct GpuDisplayMode {
    u32 width;
    u32 height;
    u32 refreshRate;
    u8 bitsPerPixel;
};

struct GpuBuffer {
    u32 id;
    GpuBufferType type;
    u64 size;
    u64 gpuAddress;
    void* cpuAddress;
    bool mapped;
    bool active;
};

struct GpuTexture {
    u32 id;
    u32 width;
    u32 height;
    u32 depth;
    u32 mipLevels;
    GpuTextureFormat format;
    u64 gpuAddress;
    bool active;
};

struct GpuRenderTarget {
    u32 id;
    u32 width;
    u32 height;
    GpuTexture* colorTexture;
    GpuTexture* depthTexture;
    bool active;
};

struct GpuViewport {
    i32 x;
    i32 y;
    u32 width;
    u32 height;
    float minDepth;
    float maxDepth;
};

struct GpuScissor {
    i32 x;
    i32 y;
    u32 width;
    u32 height;
};

struct GpuDrawCommand {
    GpuPrimitiveType primitive;
    u32 vertexCount;
    u32 instanceCount;
    u32 firstVertex;
    u32 firstInstance;
};

struct GpuDrawIndexedCommand {
    GpuPrimitiveType primitive;
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    i32 vertexOffset;
    u32 firstInstance;
};

struct GpuBlitCommand {
    GpuTexture* source;
    GpuTexture* dest;
    i32 srcX, srcY;
    u32 srcWidth, srcHeight;
    i32 dstX, dstY;
    u32 dstWidth, dstHeight;
};

struct GpuContext {
    u32 id;
    u32 deviceId;
    GpuRenderTarget* currentTarget;
    GpuViewport viewport;
    GpuScissor scissor;
    GpuBlendMode blendMode;
    bool depthTest;
    bool depthWrite;
    bool active;
};

struct GpuCommandBuffer {
    u32 id;
    u32 contextId;
    void* commands;
    usize commandSize;
    usize commandCapacity;
    bool recording;
    bool active;
};

struct GpuDevice {
    u32 id;
    GpuDeviceInfo info;
    u64 mmioBase;
    u64 vramBase;
    u64 vramSize;
    u8 irq;
    GpuDisplayMode currentMode;
    void* framebuffer;
    bool active;
    void* driverData;
};

class GpuDriver {
public:
    static void initialize();
    
    static bool registerDevice(GpuVendor vendor, u64 mmioBase, u64 vramBase, u64 vramSize, u8 irq);
    static void unregisterDevice(u32 deviceId);
    
    static GpuDevice* getDevice(u32 deviceId);
    static u32 deviceCount();
    static GpuDeviceInfo* getDeviceInfo(u32 deviceId);
    
    static bool setDisplayMode(u32 deviceId, u32 width, u32 height, u8 bpp);
    static GpuDisplayMode* getCurrentMode(u32 deviceId);
    static void* getFramebuffer(u32 deviceId);
    
    static i32 createContext(u32 deviceId);
    static bool destroyContext(i32 contextId);
    static GpuContext* getContext(i32 contextId);
    
    static i32 createBuffer(i32 contextId, GpuBufferType type, u64 size);
    static bool destroyBuffer(i32 bufferId);
    static void* mapBuffer(i32 bufferId);
    static bool unmapBuffer(i32 bufferId);
    static bool updateBuffer(i32 bufferId, const void* data, u64 offset, u64 size);
    
    static i32 createTexture(i32 contextId, u32 width, u32 height, GpuTextureFormat format);
    static bool destroyTexture(i32 textureId);
    static bool updateTexture(i32 textureId, const void* data, u32 x, u32 y, u32 width, u32 height);
    
    static i32 createRenderTarget(i32 contextId, u32 width, u32 height, bool hasDepth);
    static bool destroyRenderTarget(i32 targetId);
    static bool setRenderTarget(i32 contextId, i32 targetId);
    
    static bool setViewport(i32 contextId, i32 x, i32 y, u32 width, u32 height);
    static bool setScissor(i32 contextId, i32 x, i32 y, u32 width, u32 height);
    static bool setBlendMode(i32 contextId, GpuBlendMode mode);
    static bool setDepthTest(i32 contextId, bool enable, bool write);
    
    static bool clear(i32 contextId, u32 color, float depth);
    static bool draw(i32 contextId, const GpuDrawCommand* cmd);
    static bool drawIndexed(i32 contextId, const GpuDrawIndexedCommand* cmd);
    static bool blit(i32 contextId, const GpuBlitCommand* cmd);
    
    static bool present(u32 deviceId);
    static bool waitVSync(u32 deviceId);
    
    static bool fillRect(u32 deviceId, i32 x, i32 y, u32 width, u32 height, u32 color);
    static bool copyRect(u32 deviceId, i32 srcX, i32 srcY, i32 dstX, i32 dstY, u32 width, u32 height);
    static bool drawLine(u32 deviceId, i32 x1, i32 y1, i32 x2, i32 y2, u32 color);
    
    static bool isInitialized();

private:
    static bool initBochsVga(GpuDevice* device);
    static bool initVirtioGpu(GpuDevice* device);
    static bool initVmwareSvga(GpuDevice* device);
    
    static void bochsWriteReg(GpuDevice* device, u16 reg, u16 value);
    static u16 bochsReadReg(GpuDevice* device, u16 reg);
    
    static GpuDevice sDevices[MAX_GPU_DEVICES];
    static GpuContext sContexts[MAX_GPU_CONTEXTS];
    static GpuBuffer sBuffers[MAX_GPU_BUFFERS];
    static GpuTexture sTextures[MAX_GPU_BUFFERS];
    static GpuRenderTarget sRenderTargets[MAX_GPU_CONTEXTS];
    static u32 sDeviceCount;
    static u32 sContextCount;
    static u32 sBufferCount;
    static u32 sTextureCount;
    static u32 sRenderTargetCount;
    static bool sInitialized;
};

}
