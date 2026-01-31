#include "../../include/drivers/gpu.hpp"
#include "../../include/cpu/io.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::drivers {

namespace {

void memset(void* dest, u8 value, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    for (usize i = 0; i < size; i++) {
        d[i] = value;
    }
}

void memcpy(void* dest, const void* src, usize size) {
    u8* d = reinterpret_cast<u8*>(dest);
    const u8* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

void strcpy(char* dest, const char* src, usize maxLen) {
    usize i = 0;
    while (src[i] && i < maxLen - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

i32 abs(i32 x) {
    return x < 0 ? -x : x;
}

}

constexpr u16 VBE_DISPI_IOPORT_INDEX = 0x01CE;
constexpr u16 VBE_DISPI_IOPORT_DATA = 0x01CF;

constexpr u16 VBE_DISPI_INDEX_ID = 0;
constexpr u16 VBE_DISPI_INDEX_XRES = 1;
constexpr u16 VBE_DISPI_INDEX_YRES = 2;
constexpr u16 VBE_DISPI_INDEX_BPP = 3;
constexpr u16 VBE_DISPI_INDEX_ENABLE = 4;
constexpr u16 VBE_DISPI_INDEX_BANK = 5;
constexpr u16 VBE_DISPI_INDEX_VIRT_WIDTH = 6;
constexpr u16 VBE_DISPI_INDEX_VIRT_HEIGHT = 7;
constexpr u16 VBE_DISPI_INDEX_X_OFFSET = 8;
constexpr u16 VBE_DISPI_INDEX_Y_OFFSET = 9;

constexpr u16 VBE_DISPI_DISABLED = 0x00;
constexpr u16 VBE_DISPI_ENABLED = 0x01;
constexpr u16 VBE_DISPI_LFB_ENABLED = 0x40;

GpuDevice GpuDriver::sDevices[MAX_GPU_DEVICES];
GpuContext GpuDriver::sContexts[MAX_GPU_CONTEXTS];
GpuBuffer GpuDriver::sBuffers[MAX_GPU_BUFFERS];
GpuTexture GpuDriver::sTextures[MAX_GPU_BUFFERS];
GpuRenderTarget GpuDriver::sRenderTargets[MAX_GPU_CONTEXTS];
u32 GpuDriver::sDeviceCount = 0;
u32 GpuDriver::sContextCount = 0;
u32 GpuDriver::sBufferCount = 0;
u32 GpuDriver::sTextureCount = 0;
u32 GpuDriver::sRenderTargetCount = 0;
bool GpuDriver::sInitialized = false;

void GpuDriver::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_GPU_DEVICES; i++) {
        memset(&sDevices[i], 0, sizeof(GpuDevice));
        sDevices[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_GPU_CONTEXTS; i++) {
        memset(&sContexts[i], 0, sizeof(GpuContext));
        sContexts[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_GPU_BUFFERS; i++) {
        memset(&sBuffers[i], 0, sizeof(GpuBuffer));
        sBuffers[i].active = false;
        memset(&sTextures[i], 0, sizeof(GpuTexture));
        sTextures[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_GPU_CONTEXTS; i++) {
        memset(&sRenderTargets[i], 0, sizeof(GpuRenderTarget));
        sRenderTargets[i].active = false;
    }
    
    sDeviceCount = 0;
    sContextCount = 0;
    sBufferCount = 0;
    sTextureCount = 0;
    sRenderTargetCount = 0;
    sInitialized = true;
}

bool GpuDriver::registerDevice(GpuVendor vendor, u64 mmioBase, u64 vramBase, u64 vramSize, u8 irq) {
    if (!sInitialized) return false;
    if (sDeviceCount >= MAX_GPU_DEVICES) return false;
    
    GpuDevice* device = nullptr;
    for (u32 i = 0; i < MAX_GPU_DEVICES; i++) {
        if (!sDevices[i].active) {
            device = &sDevices[i];
            device->id = i;
            break;
        }
    }
    
    if (!device) return false;
    
    memset(device, 0, sizeof(GpuDevice));
    device->info.vendor = vendor;
    device->mmioBase = mmioBase;
    device->vramBase = vramBase;
    device->vramSize = vramSize;
    device->irq = irq;
    device->active = true;
    
    bool success = false;
    switch (vendor) {
        case GpuVendor::Bochs:
            success = initBochsVga(device);
            break;
        case GpuVendor::VirtIO:
            success = initVirtioGpu(device);
            break;
        case GpuVendor::VMware:
            success = initVmwareSvga(device);
            break;
        default:
            break;
    }
    
    if (success) {
        sDeviceCount++;
    } else {
        device->active = false;
    }
    
    return success;
}

void GpuDriver::unregisterDevice(u32 deviceId) {
    if (!sInitialized) return;
    if (deviceId >= MAX_GPU_DEVICES) return;
    
    GpuDevice* device = &sDevices[deviceId];
    if (!device->active) return;
    
    for (u32 i = 0; i < MAX_GPU_CONTEXTS; i++) {
        if (sContexts[i].active && sContexts[i].deviceId == deviceId) {
            destroyContext(static_cast<i32>(i));
        }
    }
    
    device->active = false;
    sDeviceCount--;
}

GpuDevice* GpuDriver::getDevice(u32 deviceId) {
    if (!sInitialized) return nullptr;
    if (deviceId >= MAX_GPU_DEVICES) return nullptr;
    
    if (sDevices[deviceId].active) {
        return &sDevices[deviceId];
    }
    
    return nullptr;
}

u32 GpuDriver::deviceCount() {
    return sDeviceCount;
}

GpuDeviceInfo* GpuDriver::getDeviceInfo(u32 deviceId) {
    GpuDevice* device = getDevice(deviceId);
    if (!device) return nullptr;
    return &device->info;
}

bool GpuDriver::setDisplayMode(u32 deviceId, u32 width, u32 height, u8 bpp) {
    if (!sInitialized) return false;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device) return false;
    
    switch (device->info.vendor) {
        case GpuVendor::Bochs:
            bochsWriteReg(device, VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
            bochsWriteReg(device, VBE_DISPI_INDEX_XRES, static_cast<u16>(width));
            bochsWriteReg(device, VBE_DISPI_INDEX_YRES, static_cast<u16>(height));
            bochsWriteReg(device, VBE_DISPI_INDEX_BPP, bpp);
            bochsWriteReg(device, VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
            break;
        default:
            return false;
    }
    
    device->currentMode.width = width;
    device->currentMode.height = height;
    device->currentMode.bitsPerPixel = bpp;
    device->currentMode.refreshRate = 60;
    
    device->framebuffer = reinterpret_cast<void*>(device->vramBase);
    
    return true;
}

GpuDisplayMode* GpuDriver::getCurrentMode(u32 deviceId) {
    GpuDevice* device = getDevice(deviceId);
    if (!device) return nullptr;
    return &device->currentMode;
}

void* GpuDriver::getFramebuffer(u32 deviceId) {
    GpuDevice* device = getDevice(deviceId);
    if (!device) return nullptr;
    return device->framebuffer;
}

i32 GpuDriver::createContext(u32 deviceId) {
    if (!sInitialized) return -1;
    if (sContextCount >= MAX_GPU_CONTEXTS) return -1;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device) return -1;
    
    GpuContext* context = nullptr;
    i32 contextId = -1;
    for (u32 i = 0; i < MAX_GPU_CONTEXTS; i++) {
        if (!sContexts[i].active) {
            context = &sContexts[i];
            contextId = static_cast<i32>(i);
            break;
        }
    }
    
    if (!context) return -1;
    
    memset(context, 0, sizeof(GpuContext));
    context->id = static_cast<u32>(contextId);
    context->deviceId = deviceId;
    context->currentTarget = nullptr;
    context->viewport.x = 0;
    context->viewport.y = 0;
    context->viewport.width = device->currentMode.width;
    context->viewport.height = device->currentMode.height;
    context->viewport.minDepth = 0.0f;
    context->viewport.maxDepth = 1.0f;
    context->scissor.x = 0;
    context->scissor.y = 0;
    context->scissor.width = device->currentMode.width;
    context->scissor.height = device->currentMode.height;
    context->blendMode = GpuBlendMode::None;
    context->depthTest = false;
    context->depthWrite = false;
    context->active = true;
    
    sContextCount++;
    
    return contextId;
}

bool GpuDriver::destroyContext(i32 contextId) {
    if (!sInitialized) return false;
    if (contextId < 0 || contextId >= static_cast<i32>(MAX_GPU_CONTEXTS)) return false;
    
    GpuContext* context = &sContexts[contextId];
    if (!context->active) return false;
    
    context->active = false;
    sContextCount--;
    
    return true;
}

GpuContext* GpuDriver::getContext(i32 contextId) {
    if (!sInitialized) return nullptr;
    if (contextId < 0 || contextId >= static_cast<i32>(MAX_GPU_CONTEXTS)) return nullptr;
    
    if (sContexts[contextId].active) {
        return &sContexts[contextId];
    }
    
    return nullptr;
}

i32 GpuDriver::createBuffer(i32 contextId, GpuBufferType type, u64 size) {
    if (!sInitialized) return -1;
    if (sBufferCount >= MAX_GPU_BUFFERS) return -1;
    
    GpuContext* context = getContext(contextId);
    if (!context) return -1;
    
    GpuBuffer* buffer = nullptr;
    i32 bufferId = -1;
    for (u32 i = 0; i < MAX_GPU_BUFFERS; i++) {
        if (!sBuffers[i].active) {
            buffer = &sBuffers[i];
            bufferId = static_cast<i32>(i);
            break;
        }
    }
    
    if (!buffer) return -1;
    
    usize pages = (size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
    void* mem = memory::PMM::allocatePages(pages);
    if (!mem) return -1;
    
    memset(buffer, 0, sizeof(GpuBuffer));
    buffer->id = static_cast<u32>(bufferId);
    buffer->type = type;
    buffer->size = size;
    buffer->gpuAddress = reinterpret_cast<u64>(mem);
    buffer->cpuAddress = mem;
    buffer->mapped = false;
    buffer->active = true;
    
    sBufferCount++;
    
    return bufferId;
}

bool GpuDriver::destroyBuffer(i32 bufferId) {
    if (!sInitialized) return false;
    if (bufferId < 0 || bufferId >= static_cast<i32>(MAX_GPU_BUFFERS)) return false;
    
    GpuBuffer* buffer = &sBuffers[bufferId];
    if (!buffer->active) return false;
    
    if (buffer->cpuAddress) {
        usize pages = (buffer->size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        memory::PMM::freePages(buffer->cpuAddress, pages);
    }
    
    buffer->active = false;
    sBufferCount--;
    
    return true;
}

void* GpuDriver::mapBuffer(i32 bufferId) {
    if (!sInitialized) return nullptr;
    if (bufferId < 0 || bufferId >= static_cast<i32>(MAX_GPU_BUFFERS)) return nullptr;
    
    GpuBuffer* buffer = &sBuffers[bufferId];
    if (!buffer->active) return nullptr;
    
    buffer->mapped = true;
    return buffer->cpuAddress;
}

bool GpuDriver::unmapBuffer(i32 bufferId) {
    if (!sInitialized) return false;
    if (bufferId < 0 || bufferId >= static_cast<i32>(MAX_GPU_BUFFERS)) return false;
    
    GpuBuffer* buffer = &sBuffers[bufferId];
    if (!buffer->active) return false;
    
    buffer->mapped = false;
    return true;
}

bool GpuDriver::updateBuffer(i32 bufferId, const void* data, u64 offset, u64 size) {
    if (!sInitialized || !data) return false;
    if (bufferId < 0 || bufferId >= static_cast<i32>(MAX_GPU_BUFFERS)) return false;
    
    GpuBuffer* buffer = &sBuffers[bufferId];
    if (!buffer->active) return false;
    if (offset + size > buffer->size) return false;
    
    u8* dst = reinterpret_cast<u8*>(buffer->cpuAddress) + offset;
    memcpy(dst, data, size);
    
    return true;
}

i32 GpuDriver::createTexture(i32 contextId, u32 width, u32 height, GpuTextureFormat format) {
    if (!sInitialized) return -1;
    if (sTextureCount >= MAX_GPU_BUFFERS) return -1;
    
    GpuContext* context = getContext(contextId);
    if (!context) return -1;
    
    GpuTexture* texture = nullptr;
    i32 textureId = -1;
    for (u32 i = 0; i < MAX_GPU_BUFFERS; i++) {
        if (!sTextures[i].active) {
            texture = &sTextures[i];
            textureId = static_cast<i32>(i);
            break;
        }
    }
    
    if (!texture) return -1;
    
    u32 bytesPerPixel = 4;
    switch (format) {
        case GpuTextureFormat::R8:
            bytesPerPixel = 1;
            break;
        case GpuTextureFormat::RG8:
            bytesPerPixel = 2;
            break;
        case GpuTextureFormat::RGB8:
        case GpuTextureFormat::BGR8:
            bytesPerPixel = 3;
            break;
        case GpuTextureFormat::RGBA8:
        case GpuTextureFormat::BGRA8:
            bytesPerPixel = 4;
            break;
        default:
            bytesPerPixel = 4;
            break;
    }
    
    u64 size = static_cast<u64>(width) * height * bytesPerPixel;
    usize pages = (size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
    void* mem = memory::PMM::allocatePages(pages);
    if (!mem) return -1;
    
    memset(texture, 0, sizeof(GpuTexture));
    texture->id = static_cast<u32>(textureId);
    texture->width = width;
    texture->height = height;
    texture->depth = 1;
    texture->mipLevels = 1;
    texture->format = format;
    texture->gpuAddress = reinterpret_cast<u64>(mem);
    texture->active = true;
    
    sTextureCount++;
    
    return textureId;
}

bool GpuDriver::destroyTexture(i32 textureId) {
    if (!sInitialized) return false;
    if (textureId < 0 || textureId >= static_cast<i32>(MAX_GPU_BUFFERS)) return false;
    
    GpuTexture* texture = &sTextures[textureId];
    if (!texture->active) return false;
    
    if (texture->gpuAddress) {
        u32 bytesPerPixel = 4;
        u64 size = static_cast<u64>(texture->width) * texture->height * bytesPerPixel;
        usize pages = (size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        memory::PMM::freePages(reinterpret_cast<void*>(texture->gpuAddress), pages);
    }
    
    texture->active = false;
    sTextureCount--;
    
    return true;
}

bool GpuDriver::updateTexture(i32 textureId, const void* data, u32 x, u32 y, u32 width, u32 height) {
    if (!sInitialized || !data) return false;
    if (textureId < 0 || textureId >= static_cast<i32>(MAX_GPU_BUFFERS)) return false;
    
    GpuTexture* texture = &sTextures[textureId];
    if (!texture->active) return false;
    
    if (x + width > texture->width || y + height > texture->height) return false;
    
    u32 bytesPerPixel = 4;
    u8* dst = reinterpret_cast<u8*>(texture->gpuAddress);
    const u8* src = reinterpret_cast<const u8*>(data);
    
    for (u32 row = 0; row < height; row++) {
        u64 dstOffset = ((y + row) * texture->width + x) * bytesPerPixel;
        u64 srcOffset = row * width * bytesPerPixel;
        memcpy(dst + dstOffset, src + srcOffset, width * bytesPerPixel);
    }
    
    return true;
}

i32 GpuDriver::createRenderTarget(i32 contextId, u32 width, u32 height, bool hasDepth) {
    if (!sInitialized) return -1;
    if (sRenderTargetCount >= MAX_GPU_CONTEXTS) return -1;
    
    GpuContext* context = getContext(contextId);
    if (!context) return -1;
    
    GpuRenderTarget* target = nullptr;
    i32 targetId = -1;
    for (u32 i = 0; i < MAX_GPU_CONTEXTS; i++) {
        if (!sRenderTargets[i].active) {
            target = &sRenderTargets[i];
            targetId = static_cast<i32>(i);
            break;
        }
    }
    
    if (!target) return -1;
    
    i32 colorTexId = createTexture(contextId, width, height, GpuTextureFormat::RGBA8);
    if (colorTexId < 0) return -1;
    
    i32 depthTexId = -1;
    if (hasDepth) {
        depthTexId = createTexture(contextId, width, height, GpuTextureFormat::Depth24Stencil8);
        if (depthTexId < 0) {
            destroyTexture(colorTexId);
            return -1;
        }
    }
    
    memset(target, 0, sizeof(GpuRenderTarget));
    target->id = static_cast<u32>(targetId);
    target->width = width;
    target->height = height;
    target->colorTexture = &sTextures[colorTexId];
    target->depthTexture = hasDepth ? &sTextures[depthTexId] : nullptr;
    target->active = true;
    
    sRenderTargetCount++;
    
    return targetId;
}

bool GpuDriver::destroyRenderTarget(i32 targetId) {
    if (!sInitialized) return false;
    if (targetId < 0 || targetId >= static_cast<i32>(MAX_GPU_CONTEXTS)) return false;
    
    GpuRenderTarget* target = &sRenderTargets[targetId];
    if (!target->active) return false;
    
    if (target->colorTexture) {
        destroyTexture(static_cast<i32>(target->colorTexture->id));
    }
    if (target->depthTexture) {
        destroyTexture(static_cast<i32>(target->depthTexture->id));
    }
    
    target->active = false;
    sRenderTargetCount--;
    
    return true;
}

bool GpuDriver::setRenderTarget(i32 contextId, i32 targetId) {
    if (!sInitialized) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    if (targetId < 0) {
        context->currentTarget = nullptr;
        return true;
    }
    
    if (targetId >= static_cast<i32>(MAX_GPU_CONTEXTS)) return false;
    
    GpuRenderTarget* target = &sRenderTargets[targetId];
    if (!target->active) return false;
    
    context->currentTarget = target;
    
    return true;
}

bool GpuDriver::setViewport(i32 contextId, i32 x, i32 y, u32 width, u32 height) {
    if (!sInitialized) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    context->viewport.x = x;
    context->viewport.y = y;
    context->viewport.width = width;
    context->viewport.height = height;
    
    return true;
}

bool GpuDriver::setScissor(i32 contextId, i32 x, i32 y, u32 width, u32 height) {
    if (!sInitialized) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    context->scissor.x = x;
    context->scissor.y = y;
    context->scissor.width = width;
    context->scissor.height = height;
    
    return true;
}

bool GpuDriver::setBlendMode(i32 contextId, GpuBlendMode mode) {
    if (!sInitialized) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    context->blendMode = mode;
    
    return true;
}

bool GpuDriver::setDepthTest(i32 contextId, bool enable, bool write) {
    if (!sInitialized) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    context->depthTest = enable;
    context->depthWrite = write;
    
    return true;
}

bool GpuDriver::clear(i32 contextId, u32 color, float depth) {
    if (!sInitialized) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    GpuDevice* device = getDevice(context->deviceId);
    if (!device) return false;
    
    void* target = nullptr;
    u32 width = 0;
    u32 height = 0;
    
    if (context->currentTarget) {
        target = reinterpret_cast<void*>(context->currentTarget->colorTexture->gpuAddress);
        width = context->currentTarget->width;
        height = context->currentTarget->height;
    } else {
        target = device->framebuffer;
        width = device->currentMode.width;
        height = device->currentMode.height;
    }
    
    if (!target) return false;
    
    u32* pixels = reinterpret_cast<u32*>(target);
    u32 totalPixels = width * height;
    
    for (u32 i = 0; i < totalPixels; i++) {
        pixels[i] = color;
    }
    
    return true;
}

bool GpuDriver::draw(i32 contextId, const GpuDrawCommand* cmd) {
    if (!sInitialized || !cmd) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    return true;
}

bool GpuDriver::drawIndexed(i32 contextId, const GpuDrawIndexedCommand* cmd) {
    if (!sInitialized || !cmd) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    return true;
}

bool GpuDriver::blit(i32 contextId, const GpuBlitCommand* cmd) {
    if (!sInitialized || !cmd) return false;
    
    GpuContext* context = getContext(contextId);
    if (!context) return false;
    
    if (!cmd->source || !cmd->dest) return false;
    
    u32* src = reinterpret_cast<u32*>(cmd->source->gpuAddress);
    u32* dst = reinterpret_cast<u32*>(cmd->dest->gpuAddress);
    
    for (u32 y = 0; y < cmd->srcHeight && y < cmd->dstHeight; y++) {
        for (u32 x = 0; x < cmd->srcWidth && x < cmd->dstWidth; x++) {
            u32 srcIdx = (cmd->srcY + y) * cmd->source->width + (cmd->srcX + x);
            u32 dstIdx = (cmd->dstY + y) * cmd->dest->width + (cmd->dstX + x);
            dst[dstIdx] = src[srcIdx];
        }
    }
    
    return true;
}

bool GpuDriver::present(u32 deviceId) {
    if (!sInitialized) return false;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device) return false;
    
    return true;
}

bool GpuDriver::waitVSync(u32 deviceId) {
    if (!sInitialized) return false;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device) return false;
    
    return true;
}

bool GpuDriver::fillRect(u32 deviceId, i32 x, i32 y, u32 width, u32 height, u32 color) {
    if (!sInitialized) return false;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device || !device->framebuffer) return false;
    
    u32* fb = reinterpret_cast<u32*>(device->framebuffer);
    u32 fbWidth = device->currentMode.width;
    u32 fbHeight = device->currentMode.height;
    
    i32 x1 = x < 0 ? 0 : x;
    i32 y1 = y < 0 ? 0 : y;
    i32 x2 = x + static_cast<i32>(width);
    i32 y2 = y + static_cast<i32>(height);
    
    if (x2 > static_cast<i32>(fbWidth)) x2 = static_cast<i32>(fbWidth);
    if (y2 > static_cast<i32>(fbHeight)) y2 = static_cast<i32>(fbHeight);
    
    for (i32 py = y1; py < y2; py++) {
        for (i32 px = x1; px < x2; px++) {
            fb[py * fbWidth + px] = color;
        }
    }
    
    return true;
}

bool GpuDriver::copyRect(u32 deviceId, i32 srcX, i32 srcY, i32 dstX, i32 dstY, u32 width, u32 height) {
    if (!sInitialized) return false;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device || !device->framebuffer) return false;
    
    u32* fb = reinterpret_cast<u32*>(device->framebuffer);
    u32 fbWidth = device->currentMode.width;
    
    if (dstY < srcY || (dstY == srcY && dstX < srcX)) {
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                u32 srcIdx = (srcY + y) * fbWidth + (srcX + x);
                u32 dstIdx = (dstY + y) * fbWidth + (dstX + x);
                fb[dstIdx] = fb[srcIdx];
            }
        }
    } else {
        for (i32 y = static_cast<i32>(height) - 1; y >= 0; y--) {
            for (i32 x = static_cast<i32>(width) - 1; x >= 0; x--) {
                u32 srcIdx = (srcY + y) * fbWidth + (srcX + x);
                u32 dstIdx = (dstY + y) * fbWidth + (dstX + x);
                fb[dstIdx] = fb[srcIdx];
            }
        }
    }
    
    return true;
}

bool GpuDriver::drawLine(u32 deviceId, i32 x1, i32 y1, i32 x2, i32 y2, u32 color) {
    if (!sInitialized) return false;
    
    GpuDevice* device = getDevice(deviceId);
    if (!device || !device->framebuffer) return false;
    
    u32* fb = reinterpret_cast<u32*>(device->framebuffer);
    u32 fbWidth = device->currentMode.width;
    u32 fbHeight = device->currentMode.height;
    
    i32 dx = abs(x2 - x1);
    i32 dy = abs(y2 - y1);
    i32 sx = x1 < x2 ? 1 : -1;
    i32 sy = y1 < y2 ? 1 : -1;
    i32 err = dx - dy;
    
    while (true) {
        if (x1 >= 0 && x1 < static_cast<i32>(fbWidth) &&
            y1 >= 0 && y1 < static_cast<i32>(fbHeight)) {
            fb[y1 * fbWidth + x1] = color;
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        i32 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    
    return true;
}

bool GpuDriver::isInitialized() {
    return sInitialized;
}

bool GpuDriver::initBochsVga(GpuDevice* device) {
    if (!device) return false;
    
    strcpy(device->info.name, "Bochs VGA", 64);
    device->info.vendorId = 0x1234;
    device->info.deviceId = 0x1111;
    device->info.videoMemory = device->vramSize;
    device->info.maxTextureSize = 4096;
    device->info.maxRenderTargets = 1;
    device->info.features = static_cast<u32>(GpuFeature::Acceleration2D);
    
    u16 id = bochsReadReg(device, VBE_DISPI_INDEX_ID);
    if (id < 0xB0C0 || id > 0xB0C5) {
        return false;
    }
    
    return true;
}

bool GpuDriver::initVirtioGpu(GpuDevice* device) {
    if (!device) return false;
    
    strcpy(device->info.name, "VirtIO GPU", 64);
    device->info.vendorId = 0x1AF4;
    device->info.deviceId = 0x1050;
    device->info.videoMemory = device->vramSize;
    device->info.maxTextureSize = 8192;
    device->info.maxRenderTargets = 8;
    device->info.features = static_cast<u32>(GpuFeature::Acceleration2D) |
                           static_cast<u32>(GpuFeature::Acceleration3D);
    
    return true;
}

bool GpuDriver::initVmwareSvga(GpuDevice* device) {
    if (!device) return false;
    
    strcpy(device->info.name, "VMware SVGA", 64);
    device->info.vendorId = 0x15AD;
    device->info.deviceId = 0x0405;
    device->info.videoMemory = device->vramSize;
    device->info.maxTextureSize = 8192;
    device->info.maxRenderTargets = 8;
    device->info.features = static_cast<u32>(GpuFeature::Acceleration2D) |
                           static_cast<u32>(GpuFeature::Acceleration3D);
    
    return true;
}

void GpuDriver::bochsWriteReg(GpuDevice*, u16 reg, u16 value) {
    cpu::outw(VBE_DISPI_IOPORT_INDEX, reg);
    cpu::outw(VBE_DISPI_IOPORT_DATA, value);
}

u16 GpuDriver::bochsReadReg(GpuDevice*, u16 reg) {
    cpu::outw(VBE_DISPI_IOPORT_INDEX, reg);
    return cpu::inw(VBE_DISPI_IOPORT_DATA);
}

}
