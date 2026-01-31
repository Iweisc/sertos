#pragma once

#include "../types.hpp"

namespace sertos::drivers {

constexpr u32 MAX_AUDIO_DEVICES = 8;
constexpr u32 MAX_AUDIO_STREAMS = 32;
constexpr u32 AUDIO_BUFFER_SIZE = 4096;
constexpr u32 MAX_AUDIO_CHANNELS = 8;

enum class AudioFormat : u8 {
    Unknown = 0,
    PCM_S8,
    PCM_U8,
    PCM_S16_LE,
    PCM_S16_BE,
    PCM_S24_LE,
    PCM_S24_BE,
    PCM_S32_LE,
    PCM_S32_BE,
    PCM_FLOAT32_LE,
    PCM_FLOAT32_BE
};

enum class AudioDeviceType : u8 {
    Unknown = 0,
    HDAudio,
    AC97,
    SoundBlaster,
    USB
};

enum class AudioStreamType : u8 {
    Playback = 0,
    Capture
};

enum class AudioStreamState : u8 {
    Stopped = 0,
    Running,
    Paused,
    Draining
};

struct AudioDeviceInfo {
    char name[64];
    char vendor[32];
    AudioDeviceType type;
    u32 maxChannels;
    u32 minSampleRate;
    u32 maxSampleRate;
    u32 supportedFormats;
    bool hasPlayback;
    bool hasCapture;
};

struct AudioStreamParams {
    AudioFormat format;
    u32 sampleRate;
    u8 channels;
    u32 bufferSize;
    u32 periodSize;
};

struct AudioBuffer {
    void* data;
    usize size;
    usize writePos;
    usize readPos;
    usize available;
    bool full;
};

struct AudioStream {
    u32 id;
    u32 deviceId;
    AudioStreamType type;
    AudioStreamState state;
    AudioStreamParams params;
    AudioBuffer buffer;
    u64 framesWritten;
    u64 framesRead;
    bool active;
};

struct AudioDevice {
    u32 id;
    AudioDeviceInfo info;
    u64 baseAddress;
    u8 irq;
    bool active;
    void* driverData;
};

struct HDAudioWidget {
    u32 nodeId;
    u32 type;
    u32 capabilities;
    u32 connectionList[16];
    u8 connectionCount;
};

struct HDAudioCodec {
    u32 codecId;
    u32 vendorId;
    u32 subsystemId;
    u32 revisionId;
    HDAudioWidget widgets[64];
    u8 widgetCount;
    u8 audioFunctionGroup;
};

using AudioCallback = void (*)(AudioStream* stream, void* buffer, usize frames);

class AudioDriver {
public:
    static void initialize();
    
    static bool registerDevice(AudioDeviceType type, u64 baseAddr, u8 irq);
    static void unregisterDevice(u32 deviceId);
    
    static AudioDevice* getDevice(u32 deviceId);
    static u32 deviceCount();
    static AudioDeviceInfo* getDeviceInfo(u32 deviceId);
    
    static i32 openStream(u32 deviceId, AudioStreamType type, AudioStreamParams* params);
    static bool closeStream(i32 streamId);
    static bool startStream(i32 streamId);
    static bool stopStream(i32 streamId);
    static bool pauseStream(i32 streamId);
    static bool resumeStream(i32 streamId);
    
    static i64 writeStream(i32 streamId, const void* buffer, usize frames);
    static i64 readStream(i32 streamId, void* buffer, usize frames);
    
    static bool setStreamVolume(i32 streamId, u8 volume);
    static u8 getStreamVolume(i32 streamId);
    static bool setMasterVolume(u32 deviceId, u8 volume);
    static u8 getMasterVolume(u32 deviceId);
    
    static void setCallback(i32 streamId, AudioCallback callback);
    
    static bool isInitialized();

private:
    static bool initHDAudio(AudioDevice* device);
    static bool initAC97(AudioDevice* device);
    
    static bool hdaReset(AudioDevice* device);
    static bool hdaInitCodec(AudioDevice* device, HDAudioCodec* codec);
    static u32 hdaSendCommand(AudioDevice* device, u32 codecAddr, u32 nodeId, u32 verb, u32 payload);
    static bool hdaSetupStream(AudioDevice* device, AudioStream* stream);
    
    static bool ac97Reset(AudioDevice* device);
    static u16 ac97ReadMixer(AudioDevice* device, u8 reg);
    static void ac97WriteMixer(AudioDevice* device, u8 reg, u16 value);
    
    static usize formatFrameSize(AudioFormat format, u8 channels);
    
    static AudioDevice sDevices[MAX_AUDIO_DEVICES];
    static AudioStream sStreams[MAX_AUDIO_STREAMS];
    static AudioCallback sCallbacks[MAX_AUDIO_STREAMS];
    static u32 sDeviceCount;
    static u32 sStreamCount;
    static bool sInitialized;
};

}
