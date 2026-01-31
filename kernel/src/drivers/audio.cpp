#include "../../include/drivers/audio.hpp"
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

usize min(usize a, usize b) {
    return a < b ? a : b;
}

}

AudioDevice AudioDriver::sDevices[MAX_AUDIO_DEVICES];
AudioStream AudioDriver::sStreams[MAX_AUDIO_STREAMS];
AudioCallback AudioDriver::sCallbacks[MAX_AUDIO_STREAMS];
u32 AudioDriver::sDeviceCount = 0;
u32 AudioDriver::sStreamCount = 0;
bool AudioDriver::sInitialized = false;

void AudioDriver::initialize() {
    if (sInitialized) return;
    
    for (u32 i = 0; i < MAX_AUDIO_DEVICES; i++) {
        memset(&sDevices[i], 0, sizeof(AudioDevice));
        sDevices[i].active = false;
    }
    
    for (u32 i = 0; i < MAX_AUDIO_STREAMS; i++) {
        memset(&sStreams[i], 0, sizeof(AudioStream));
        sStreams[i].active = false;
        sCallbacks[i] = nullptr;
    }
    
    sDeviceCount = 0;
    sStreamCount = 0;
    sInitialized = true;
}

bool AudioDriver::registerDevice(AudioDeviceType type, u64 baseAddr, u8 irq) {
    if (!sInitialized) return false;
    if (sDeviceCount >= MAX_AUDIO_DEVICES) return false;
    
    AudioDevice* device = nullptr;
    for (u32 i = 0; i < MAX_AUDIO_DEVICES; i++) {
        if (!sDevices[i].active) {
            device = &sDevices[i];
            device->id = i;
            break;
        }
    }
    
    if (!device) return false;
    
    memset(device, 0, sizeof(AudioDevice));
    device->info.type = type;
    device->baseAddress = baseAddr;
    device->irq = irq;
    device->active = true;
    
    bool success = false;
    switch (type) {
        case AudioDeviceType::HDAudio:
            success = initHDAudio(device);
            break;
        case AudioDeviceType::AC97:
            success = initAC97(device);
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

void AudioDriver::unregisterDevice(u32 deviceId) {
    if (!sInitialized) return;
    if (deviceId >= MAX_AUDIO_DEVICES) return;
    
    AudioDevice* device = &sDevices[deviceId];
    if (!device->active) return;
    
    for (u32 i = 0; i < MAX_AUDIO_STREAMS; i++) {
        if (sStreams[i].active && sStreams[i].deviceId == deviceId) {
            closeStream(static_cast<i32>(i));
        }
    }
    
    device->active = false;
    sDeviceCount--;
}

AudioDevice* AudioDriver::getDevice(u32 deviceId) {
    if (!sInitialized) return nullptr;
    if (deviceId >= MAX_AUDIO_DEVICES) return nullptr;
    
    if (sDevices[deviceId].active) {
        return &sDevices[deviceId];
    }
    
    return nullptr;
}

u32 AudioDriver::deviceCount() {
    return sDeviceCount;
}

AudioDeviceInfo* AudioDriver::getDeviceInfo(u32 deviceId) {
    AudioDevice* device = getDevice(deviceId);
    if (!device) return nullptr;
    return &device->info;
}

i32 AudioDriver::openStream(u32 deviceId, AudioStreamType type, AudioStreamParams* params) {
    if (!sInitialized || !params) return -1;
    if (sStreamCount >= MAX_AUDIO_STREAMS) return -1;
    
    AudioDevice* device = getDevice(deviceId);
    if (!device) return -1;
    
    if (type == AudioStreamType::Playback && !device->info.hasPlayback) return -1;
    if (type == AudioStreamType::Capture && !device->info.hasCapture) return -1;
    
    AudioStream* stream = nullptr;
    i32 streamId = -1;
    for (u32 i = 0; i < MAX_AUDIO_STREAMS; i++) {
        if (!sStreams[i].active) {
            stream = &sStreams[i];
            streamId = static_cast<i32>(i);
            break;
        }
    }
    
    if (!stream) return -1;
    
    memset(stream, 0, sizeof(AudioStream));
    stream->id = static_cast<u32>(streamId);
    stream->deviceId = deviceId;
    stream->type = type;
    stream->state = AudioStreamState::Stopped;
    stream->params = *params;
    stream->active = true;
    
    usize bufferSize = params->bufferSize;
    if (bufferSize == 0) {
        bufferSize = AUDIO_BUFFER_SIZE;
    }
    
    stream->buffer.data = memory::PMM::allocatePages((bufferSize + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE);
    if (!stream->buffer.data) {
        stream->active = false;
        return -1;
    }
    
    stream->buffer.size = bufferSize;
    stream->buffer.writePos = 0;
    stream->buffer.readPos = 0;
    stream->buffer.available = 0;
    stream->buffer.full = false;
    
    sStreamCount++;
    
    return streamId;
}

bool AudioDriver::closeStream(i32 streamId) {
    if (!sInitialized) return false;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return false;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return false;
    
    if (stream->state == AudioStreamState::Running) {
        stopStream(streamId);
    }
    
    if (stream->buffer.data) {
        memory::PMM::freePages(
            stream->buffer.data,
            (stream->buffer.size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE
        );
    }
    
    sCallbacks[streamId] = nullptr;
    stream->active = false;
    sStreamCount--;
    
    return true;
}

bool AudioDriver::startStream(i32 streamId) {
    if (!sInitialized) return false;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return false;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return false;
    
    if (stream->state == AudioStreamState::Running) return true;
    
    AudioDevice* device = getDevice(stream->deviceId);
    if (!device) return false;
    
    switch (device->info.type) {
        case AudioDeviceType::HDAudio:
            hdaSetupStream(device, stream);
            break;
        case AudioDeviceType::AC97:
            break;
        default:
            break;
    }
    
    stream->state = AudioStreamState::Running;
    
    return true;
}

bool AudioDriver::stopStream(i32 streamId) {
    if (!sInitialized) return false;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return false;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return false;
    
    stream->state = AudioStreamState::Stopped;
    stream->buffer.writePos = 0;
    stream->buffer.readPos = 0;
    stream->buffer.available = 0;
    stream->buffer.full = false;
    
    return true;
}

bool AudioDriver::pauseStream(i32 streamId) {
    if (!sInitialized) return false;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return false;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return false;
    
    if (stream->state != AudioStreamState::Running) return false;
    
    stream->state = AudioStreamState::Paused;
    
    return true;
}

bool AudioDriver::resumeStream(i32 streamId) {
    if (!sInitialized) return false;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return false;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return false;
    
    if (stream->state != AudioStreamState::Paused) return false;
    
    stream->state = AudioStreamState::Running;
    
    return true;
}

i64 AudioDriver::writeStream(i32 streamId, const void* buffer, usize frames) {
    if (!sInitialized || !buffer) return -1;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return -1;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return -1;
    if (stream->type != AudioStreamType::Playback) return -1;
    
    usize frameSize = formatFrameSize(stream->params.format, stream->params.channels);
    usize bytesToWrite = frames * frameSize;
    usize spaceAvailable = stream->buffer.size - stream->buffer.available;
    usize toWrite = min(bytesToWrite, spaceAvailable);
    
    const u8* src = reinterpret_cast<const u8*>(buffer);
    u8* dst = reinterpret_cast<u8*>(stream->buffer.data);
    
    usize written = 0;
    while (written < toWrite) {
        usize chunk = min(toWrite - written, stream->buffer.size - stream->buffer.writePos);
        memcpy(dst + stream->buffer.writePos, src + written, chunk);
        stream->buffer.writePos = (stream->buffer.writePos + chunk) % stream->buffer.size;
        written += chunk;
    }
    
    stream->buffer.available += written;
    stream->framesWritten += written / frameSize;
    
    if (stream->buffer.available >= stream->buffer.size) {
        stream->buffer.full = true;
    }
    
    return static_cast<i64>(written / frameSize);
}

i64 AudioDriver::readStream(i32 streamId, void* buffer, usize frames) {
    if (!sInitialized || !buffer) return -1;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return -1;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return -1;
    if (stream->type != AudioStreamType::Capture) return -1;
    
    usize frameSize = formatFrameSize(stream->params.format, stream->params.channels);
    usize bytesToRead = frames * frameSize;
    usize toRead = min(bytesToRead, stream->buffer.available);
    
    u8* dst = reinterpret_cast<u8*>(buffer);
    u8* src = reinterpret_cast<u8*>(stream->buffer.data);
    
    usize read = 0;
    while (read < toRead) {
        usize chunk = min(toRead - read, stream->buffer.size - stream->buffer.readPos);
        memcpy(dst + read, src + stream->buffer.readPos, chunk);
        stream->buffer.readPos = (stream->buffer.readPos + chunk) % stream->buffer.size;
        read += chunk;
    }
    
    stream->buffer.available -= read;
    stream->framesRead += read / frameSize;
    stream->buffer.full = false;
    
    return static_cast<i64>(read / frameSize);
}

bool AudioDriver::setStreamVolume(i32 streamId, u8 volume) {
    if (!sInitialized) return false;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return false;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return false;
    
    return true;
}

u8 AudioDriver::getStreamVolume(i32 streamId) {
    if (!sInitialized) return 0;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return 0;
    
    AudioStream* stream = &sStreams[streamId];
    if (!stream->active) return 0;
    
    return 100;
}

bool AudioDriver::setMasterVolume(u32 deviceId, u8 volume) {
    if (!sInitialized) return false;
    
    AudioDevice* device = getDevice(deviceId);
    if (!device) return false;
    
    switch (device->info.type) {
        case AudioDeviceType::AC97:
            ac97WriteMixer(device, 0x02, static_cast<u16>((100 - volume) | ((100 - volume) << 8)));
            break;
        case AudioDeviceType::HDAudio:
            break;
        default:
            break;
    }
    
    return true;
}

u8 AudioDriver::getMasterVolume(u32 deviceId) {
    if (!sInitialized) return 0;
    
    AudioDevice* device = getDevice(deviceId);
    if (!device) return 0;
    
    switch (device->info.type) {
        case AudioDeviceType::AC97: {
            u16 vol = ac97ReadMixer(device, 0x02);
            return static_cast<u8>(100 - (vol & 0x3F));
        }
        case AudioDeviceType::HDAudio:
            return 100;
        default:
            return 0;
    }
}

void AudioDriver::setCallback(i32 streamId, AudioCallback callback) {
    if (!sInitialized) return;
    if (streamId < 0 || streamId >= static_cast<i32>(MAX_AUDIO_STREAMS)) return;
    
    sCallbacks[streamId] = callback;
}

bool AudioDriver::isInitialized() {
    return sInitialized;
}

bool AudioDriver::initHDAudio(AudioDevice* device) {
    if (!device) return false;
    
    strcpy(device->info.name, "HD Audio Controller", 64);
    strcpy(device->info.vendor, "Generic", 32);
    device->info.maxChannels = 8;
    device->info.minSampleRate = 8000;
    device->info.maxSampleRate = 192000;
    device->info.hasPlayback = true;
    device->info.hasCapture = true;
    
    if (!hdaReset(device)) {
        return false;
    }
    
    return true;
}

bool AudioDriver::initAC97(AudioDevice* device) {
    if (!device) return false;
    
    strcpy(device->info.name, "AC97 Audio Controller", 64);
    strcpy(device->info.vendor, "Generic", 32);
    device->info.maxChannels = 2;
    device->info.minSampleRate = 8000;
    device->info.maxSampleRate = 48000;
    device->info.hasPlayback = true;
    device->info.hasCapture = true;
    
    if (!ac97Reset(device)) {
        return false;
    }
    
    return true;
}

bool AudioDriver::hdaReset(AudioDevice* device) {
    if (!device) return false;
    
    volatile u32* regs = reinterpret_cast<volatile u32*>(device->baseAddress);
    
    regs[0x08 / 4] = 0;
    
    for (int i = 0; i < 1000; i++) {
        if ((regs[0x08 / 4] & 1) == 0) break;
    }
    
    regs[0x08 / 4] = 1;
    
    for (int i = 0; i < 1000; i++) {
        if (regs[0x08 / 4] & 1) break;
    }
    
    return true;
}

bool AudioDriver::hdaInitCodec(AudioDevice*, HDAudioCodec*) {
    return true;
}

u32 AudioDriver::hdaSendCommand(AudioDevice* device, u32 codecAddr, u32 nodeId, u32 verb, u32 payload) {
    if (!device) return 0;
    
    volatile u32* regs = reinterpret_cast<volatile u32*>(device->baseAddress);
    
    u32 command = (codecAddr << 28) | (nodeId << 20) | (verb << 8) | payload;
    
    regs[0x60 / 4] = command;
    
    for (int i = 0; i < 1000; i++) {
        if (regs[0x64 / 4] & 1) {
            return regs[0x64 / 4];
        }
    }
    
    return 0;
}

bool AudioDriver::hdaSetupStream(AudioDevice*, AudioStream*) {
    return true;
}

bool AudioDriver::ac97Reset(AudioDevice* device) {
    if (!device) return false;
    
    u16 basePort = static_cast<u16>(device->baseAddress);
    
    cpu::outw(basePort + 0x00, 0);
    
    for (int i = 0; i < 1000; i++) {
        cpu::inw(basePort + 0x00);
    }
    
    return true;
}

u16 AudioDriver::ac97ReadMixer(AudioDevice* device, u8 reg) {
    if (!device) return 0;
    
    u16 mixerPort = static_cast<u16>(device->baseAddress);
    return cpu::inw(mixerPort + reg);
}

void AudioDriver::ac97WriteMixer(AudioDevice* device, u8 reg, u16 value) {
    if (!device) return;
    
    u16 mixerPort = static_cast<u16>(device->baseAddress);
    cpu::outw(mixerPort + reg, value);
}

usize AudioDriver::formatFrameSize(AudioFormat format, u8 channels) {
    usize sampleSize = 0;
    
    switch (format) {
        case AudioFormat::PCM_S8:
        case AudioFormat::PCM_U8:
            sampleSize = 1;
            break;
        case AudioFormat::PCM_S16_LE:
        case AudioFormat::PCM_S16_BE:
            sampleSize = 2;
            break;
        case AudioFormat::PCM_S24_LE:
        case AudioFormat::PCM_S24_BE:
            sampleSize = 3;
            break;
        case AudioFormat::PCM_S32_LE:
        case AudioFormat::PCM_S32_BE:
        case AudioFormat::PCM_FLOAT32_LE:
        case AudioFormat::PCM_FLOAT32_BE:
            sampleSize = 4;
            break;
        default:
            sampleSize = 2;
            break;
    }
    
    return sampleSize * channels;
}

}
