/* macOS playback through the default-output HAL unit. SDL's AudioQueue
 * backend keeps at least 15 ms of buffers queued twice over; the HAL render
 * callback reads the bounded ring directly at the device's I/O period. */
#include "audio_coreaudio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <stdio.h>
#include <stdlib.h>

struct m8_coreaudio {
    AudioUnit unit;
    AudioObjectID device;
    m8_coreaudio_render render;
    void *context;
    int started;
};

static int fail(char *error, size_t capacity, const char *call, OSStatus status) {
    if (error && capacity) snprintf(error, capacity, "Core Audio %s failed: status=%d", call, (int)status);
    return -1;
}

static OSStatus render(void *context, AudioUnitRenderActionFlags *flags,
                       const AudioTimeStamp *time, UInt32 bus, UInt32 frames,
                       AudioBufferList *data) {
    (void)flags; (void)time; (void)bus;
    m8_coreaudio *c = context;
    /* One interleaved buffer: the unit converts S16 stereo to the device. */
    c->render(c->context, data->mBuffers[0].mData, frames);
    return noErr;
}

static OSStatus device_property(AudioObjectID device, AudioObjectPropertySelector selector,
                                AudioObjectPropertyScope scope, void *value, UInt32 size) {
    AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
    return AudioObjectGetPropertyData(device, &address, 0, NULL, &size, value);
}

m8_coreaudio *m8_coreaudio_open(m8_coreaudio_render callback, void *context,
                                uint32_t period, char *error, size_t capacity) {
    m8_coreaudio *c = calloc(1, sizeof *c);
    if (!c) { if (error && capacity) snprintf(error, capacity, "Cannot allocate Core Audio output"); return NULL; }
    c->render = callback; c->context = context;
    AudioComponentDescription description = {
        .componentType = kAudioUnitType_Output, .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple};
    AudioComponent component = AudioComponentFindNext(NULL, &description);
    OSStatus status = component ? AudioComponentInstanceNew(component, &c->unit) : kAudioUnitErr_InvalidElement;
    if (status) { fail(error, capacity, "AudioComponentInstanceNew", status); goto failed; }
    AudioStreamBasicDescription format = {
        .mSampleRate = 44100, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = 4, .mFramesPerPacket = 1, .mBytesPerFrame = 4,
        .mChannelsPerFrame = 2, .mBitsPerChannel = 16};
    status = AudioUnitSetProperty(c->unit, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 0, &format, sizeof format);
    if (status) { fail(error, capacity, "StreamFormat", status); goto failed; }
    AURenderCallbackStruct input = {render, c};
    status = AudioUnitSetProperty(c->unit, kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input, 0, &input, sizeof input);
    if (status) { fail(error, capacity, "SetRenderCallback", status); goto failed; }
    UInt32 size = sizeof c->device;
    status = AudioUnitGetProperty(c->unit, kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global, 0, &c->device, &size);
    if (status) { fail(error, capacity, "CurrentDevice", status); goto failed; }
    if (period) {
        /* The period applies at the device rate; keep the requested duration. */
        Float64 rate = 44100;
        device_property(c->device, kAudioDevicePropertyNominalSampleRate,
                        kAudioObjectPropertyScopeGlobal, &rate, sizeof rate);
        AudioValueRange range = {0, 0};
        UInt32 frames = (UInt32)(period * rate / 44100 + .5);
        if (!device_property(c->device, kAudioDevicePropertyBufferFrameSizeRange,
                             kAudioObjectPropertyScopeGlobal, &range, sizeof range) && range.mMaximum > 0) {
            if (frames < range.mMinimum) frames = (UInt32)range.mMinimum;
            if (frames > range.mMaximum) frames = (UInt32)range.mMaximum;
        }
        AudioObjectPropertyAddress address = {kAudioDevicePropertyBufferFrameSize,
            kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        /* A shared device can refuse; its current period remains valid. */
        AudioObjectSetPropertyData(c->device, &address, 0, NULL, sizeof frames, &frames);
    }
    status = AudioUnitInitialize(c->unit);
    if (status) { fail(error, capacity, "AudioUnitInitialize", status); goto failed; }
    return c;
failed:
    m8_coreaudio_close(c);
    return NULL;
}

int m8_coreaudio_start(m8_coreaudio *c, char *error, size_t capacity) {
    if (c->started) return 0;
    OSStatus status = AudioOutputUnitStart(c->unit);
    if (status) return fail(error, capacity, "AudioOutputUnitStart", status);
    c->started = 1;
    return 0;
}

void m8_coreaudio_stop(m8_coreaudio *c) {
    if (c && c->started) { AudioOutputUnitStop(c->unit); c->started = 0; }
}

void m8_coreaudio_close(m8_coreaudio *c) {
    if (!c) return;
    m8_coreaudio_stop(c);
    if (c->unit) { AudioUnitUninitialize(c->unit); AudioComponentInstanceDispose(c->unit); }
    free(c);
}

void m8_coreaudio_format(m8_coreaudio *c, uint32_t *frames, uint32_t *rate, double *latency_ms) {
    UInt32 period = 0, device_latency = 0, safety = 0, stream_latency = 0;
    Float64 nominal = 0;
    AudioObjectID stream = 0;
    device_property(c->device, kAudioDevicePropertyBufferFrameSize,
                    kAudioObjectPropertyScopeGlobal, &period, sizeof period);
    device_property(c->device, kAudioDevicePropertyNominalSampleRate,
                    kAudioObjectPropertyScopeGlobal, &nominal, sizeof nominal);
    device_property(c->device, kAudioDevicePropertyLatency,
                    kAudioObjectPropertyScopeOutput, &device_latency, sizeof device_latency);
    device_property(c->device, kAudioDevicePropertySafetyOffset,
                    kAudioObjectPropertyScopeOutput, &safety, sizeof safety);
    if (!device_property(c->device, kAudioDevicePropertyStreams,
                         kAudioObjectPropertyScopeOutput, &stream, sizeof stream) && stream)
        device_property(stream, kAudioStreamPropertyLatency,
                        kAudioObjectPropertyScopeGlobal, &stream_latency, sizeof stream_latency);
    if (frames) *frames = period;
    if (rate) *rate = (uint32_t)nominal;
    /* Output presentation delay after the render callback returns. */
    if (latency_ms) *latency_ms = nominal > 0 ?
        (double)(period + device_latency + safety + stream_latency) * 1000 / nominal : 0;
}
