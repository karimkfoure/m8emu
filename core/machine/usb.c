#include "internal.h"
#include <stdlib.h>
#include <string.h>

/* ChipIdea device model. Register and transfer order follow usbdev.py. */
#define USB1 UINT32_C(0x402e0000)
#define GPTIMER0LD 0x080u
#define GPTIMER0CTRL 0x084u
#define USBCMD 0x140u
#define USBSTS 0x144u
#define USBINTR 0x148u
#define DEVICEADDR 0x154u
#define ENDPTLISTADDR 0x158u
#define BURSTSIZE 0x160u
#define PORTSC1 0x184u
#define USBMODE 0x1a8u
#define ENDPTSETUPSTAT 0x1acu
#define ENDPTPRIME 0x1b0u
#define ENDPTFLUSH 0x1b4u
#define ENDPTSTAT 0x1b8u
#define ENDPTCOMPLETE 0x1bcu
#define ENDPTCTRL0 0x1c0u
#define CMD_RS (UINT32_C(1) << 0)
#define CMD_RST (UINT32_C(1) << 1)
#define CMD_SUTW (UINT32_C(1) << 13)
#define CMD_ATDTW (UINT32_C(1) << 14)
#define STS_UI (UINT32_C(1) << 0)
#define STS_PCI (UINT32_C(1) << 2)
#define STS_URI (UINT32_C(1) << 6)
#define STS_SRI (UINT32_C(1) << 7)
#define STS_TI0 (UINT32_C(1) << 24)
#define INTR_SRE (UINT32_C(1) << 7)
#define INTR_TIE0 (UINT32_C(1) << 24)
#define GPT_RUN (UINT32_C(1) << 31)
#define ACTIVE (UINT32_C(1) << 7)
#define TOTAL_BYTES UINT32_C(0x7fff0000)
#define USB_PERIOD UINT32_C(150000)
#define ENUM_RESET_AT UINT32_C(12000000)
#define AUDIO_INPUT_FRAMES 16384u
#define REG(u, offset) ((u)->regs[(offset) / 4])
#define MIDI_EP 4u
#define MIDI_CAP 4096u

typedef struct {
    m8rt_midi_position position;
    uint8_t bytes[3], count;
} midi_event;

struct m8rt_usb {
    uint32_t regs[0x200 / 4];
    bool written[0x200 / 4];
    uint32_t qh_base, last_cyccnt, pcm_last_drain, gpt_arm_at;
    uint64_t insns_since_rs, in_bytes, pcm_bytes;
    bool running, reset_injected, config_injected, dtr_injected;
    bool boot_done, pcm_capture, gpt_armed;
    m8rt_queue out;
    uint8_t audio_input[AUDIO_INPUT_FRAMES * 4];
    uint64_t audio_written, audio_read, audio_silence, audio_packets;
    uint32_t audio_last_packet, audio_fraction;
    bool audio_enabled, audio_interface_setting;
    bool midi_enabled;
    uint8_t midi_in[MIDI_CAP][4];
    midi_event midi_out[MIDI_CAP];
    unsigned midi_in_head, midi_in_count, midi_out_head, midi_out_count, midi_read_offset;
};

static unsigned midi_length(unsigned cin)
{
    static const uint8_t lengths[16] = {0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};
    return lengths[cin & 15];
}

static bool emit_midi(m8rt *rt, const uint8_t *payload, unsigned count)
{
    m8rt_usb *u = rt->usb;
    if (!u->midi_enabled)
        return true;
    if (count % 4) {
        m8rt_fail(rt, "USB MIDI IN length is not a multiple of 4: count=%u", count);
        return false;
    }
    unsigned events = 0;
    for (unsigned i = 0; i < count; i += 4) {
        if (payload[i] >> 4 || !midi_length(payload[i])) {
            m8rt_fail(rt, "USB MIDI IN unsupported header: offset=%u header=%#x", i, payload[i]);
            return false;
        }
        ++events;
    }
    if (events > MIDI_CAP - u->midi_out_count) {
        m8rt_fail(rt, "USB MIDI IN queue full: required=%u queued=%u capacity=%u",
                   events, u->midi_out_count, MIDI_CAP);
        return false;
    }
    for (unsigned i = 0; i < count; i += 4) {
        midi_event *event = &u->midi_out[(u->midi_out_head + u->midi_out_count++) % MIDI_CAP];
        event->count = (uint8_t)midi_length(payload[i]);
        memcpy(event->bytes, payload + i + 1, event->count);
        event->position = (m8rt_midi_position){
            (rt->local_selected ? rt->local_total : rt->pcm_total) / 4, rt->guest_cycles};
    }
    return true;
}

static bool u32(m8rt *rt, uint32_t addr, uint32_t *value, bool required)
{
    uint8_t bytes[4];
    if (m8rt_mem_read(rt, addr, bytes, sizeof(bytes))) {
        if (required)
            m8rt_fail(rt, "USB descriptor read at %#x", addr);
        return false;
    }
    *value = m8rt_le32(bytes);
    return true;
}

static bool w32(m8rt *rt, uint32_t addr, uint32_t value, bool required)
{
    uint8_t bytes[4];
    m8rt_st32(bytes, value);
    if (m8rt_mem_write(rt, addr, bytes, sizeof(bytes))) {
        if (required)
            m8rt_fail(rt, "USB descriptor write at %#x", addr);
        return false;
    }
    return true;
}

static bool ram_buffer(uint32_t addr)
{
    return (addr >= UINT32_C(0x20000000) && addr < UINT32_C(0x20300000)) ||
           (addr >= UINT32_C(0x00000020) && addr < UINT32_C(0x00080000));
}

static void irq(m8rt *rt)
{
    /* Delivery remains at the core's safe guest boundary. */
    rt->usb_pending = true;
}

static void raise_ui(m8rt *rt, unsigned ep)
{
    if (ep < 5 || (ep == 5 && (rt->usb->pcm_capture || rt->usb->audio_enabled))) {
        REG(rt->usb, USBSTS) |= STS_UI;
        irq(rt);
    }
}

static uint32_t qh(m8rt_usb *u, unsigned index)
{
    return u->qh_base + index * 64;
}

static void complete_in(m8rt *rt, unsigned ep)
{
    m8rt_usb *u = rt->usb;
    if (ep >= 6)
        return;
    if (ep == 5) {
        if (!u->pcm_capture || !u->boot_done ||
            (uint32_t)(u->last_cyccnt - u->pcm_last_drain) < USB_PERIOD)
            return;
        /* PRIME advances the phase even when the queue turns out empty. */
        u->pcm_last_drain = u->last_cyccnt;
    }
    uint32_t head;
    if (!u32(rt, qh(u, ep * 2 + 1) + 8, &head, true))
        return;
    uint32_t dtd = head & ~UINT32_C(0x1f);
    if ((head & 1) || !dtd) {
        if (!ep) {
            REG(u, ENDPTCOMPLETE) |= UINT32_C(1) << (16 + ep);
            raise_ui(rt, ep);
        }
        return;
    }
    unsigned retired = 0;
    for (unsigned guard = 0; dtd && guard < 16; ++guard) {
        uint32_t token;
        if (!u32(rt, dtd + 4, &token, true))
            return;
        if (token & ACTIVE) {
            uint32_t count = (token >> 16) & 0x7fff, buffer;
            if (!u32(rt, dtd + 8, &buffer, true))
                return;
            if (count && buffer && count <= 2048 && ram_buffer(buffer)) {
                uint8_t payload[2048];
                if (m8rt_mem_read(rt, buffer, payload, count)) {
                    m8rt_fail(rt, "USB IN payload read at %#x, %u bytes", buffer, count);
                    return;
                }
                u->in_bytes += count;
                if (ep == 3)
                    m8rt_cdc_emit(rt, payload, count);
                else if (ep == MIDI_EP && !emit_midi(rt, payload, count))
                    return;
                else if (ep == 5) {
                    u->pcm_bytes += count;
                    m8rt_pcm_emit(rt, payload, count);
                }
            }
            if (!w32(rt, dtd + 4, token & ~ACTIVE & ~TOTAL_BYTES, true))
                return;
            ++retired;
        }
        uint32_t next;
        if (!u32(rt, dtd, &next, true))
            return;
        if (next & 1)
            break;
        dtd = next & ~UINT32_C(0x1f);
    }
    if (retired) {
        uint32_t bit = UINT32_C(1) << (16 + ep);
        REG(u, ENDPTCOMPLETE) |= bit;
        REG(u, ENDPTSTAT) &= ~bit;
        raise_ui(rt, ep);
    }
}

static void drain_bulk(m8rt *rt, unsigned ep)
{
    m8rt_usb *u = rt->usb;
    uint32_t dtd;
    if (!u->qh_base || !u32(rt, qh(u, ep * 2 + 1) + 48, &dtd, false))
        return;
    unsigned retired = 0;
    for (unsigned guard = 0; dtd && dtd != 1 && guard < 16; ++guard) {
        uint32_t token;
        if ((dtd & 0x1f) || !u32(rt, dtd + 4, &token, false))
            break;
        if (token & ACTIVE) {
            uint32_t count = (token >> 16) & 0x7fff, buffer = 0;
            (void)u32(rt, dtd + 8, &buffer, false);
            if (count && buffer && count <= 2048 && ram_buffer(buffer)) {
                uint8_t payload[2048];
                if (!m8rt_mem_read(rt, buffer, payload, count)) {
                    u->in_bytes += count;
                    if (ep == MIDI_EP) {
                        if (!emit_midi(rt, payload, count))
                            return;
                    } else {
                        m8rt_cdc_emit(rt, payload, count);
                    }
                }
            }
            (void)w32(rt, dtd + 4, token & ~ACTIVE & ~TOTAL_BYTES, false);
            ++retired;
        }
        uint32_t next;
        if (!u32(rt, dtd, &next, false) || (next & 1))
            break;
        dtd = next & ~UINT32_C(0x1f);
    }
    if (retired) {
        REG(u, ENDPTCOMPLETE) |= UINT32_C(1) << (16 + ep);
        REG(u, ENDPTSTAT) &= ~(UINT32_C(1) << (16 + ep));
        raise_ui(rt, ep);
    }
}

static void drain_pcm(m8rt *rt, uint32_t cycles)
{
    m8rt_usb *u = rt->usb;
    uint32_t dtd;
    if (!u->qh_base || !u->pcm_capture || !u->boot_done ||
        (uint32_t)(cycles - u->pcm_last_drain) < USB_PERIOD ||
        !u32(rt, qh(u, 11) + 48, &dtd, false))
        return;
    unsigned retired = 0;
    for (unsigned guard = 0; dtd && dtd != 1 && guard < 64 && !retired; ++guard) {
        uint32_t token;
        if ((dtd & 0x1f) || !u32(rt, dtd + 4, &token, false))
            break;
        if (token & ACTIVE) {
            uint32_t count = (token >> 16) & 0x7fff, buffer = 0;
            (void)u32(rt, dtd + 8, &buffer, false);
            if (count && buffer && count <= 2048 && ram_buffer(buffer)) {
                uint8_t payload[2048];
                if (!m8rt_mem_read(rt, buffer, payload, count)) {
                    u->pcm_bytes += count;
                    m8rt_pcm_emit(rt, payload, count);
                }
            }
            (void)w32(rt, dtd + 4, token & ~ACTIVE & ~TOTAL_BYTES, false);
            ++retired;
        }
        uint32_t next;
        if (!u32(rt, dtd, &next, false) || (next & 1))
            break;
        dtd = next & ~UINT32_C(0x1f);
    }
    if (retired) {
        u->pcm_last_drain = cycles;
        REG(u, ENDPTCOMPLETE) |= UINT32_C(1) << 21;
        REG(u, ENDPTSTAT) &= ~(UINT32_C(1) << 21);
        raise_ui(rt, 5);
    }
}

static bool find_active(m8rt *rt, unsigned ep, uint32_t *active)
{
    m8rt_usb *u = rt->usb;
    uint32_t heads[2];
    *active = 0;
    if (!u32(rt, qh(u, ep * 2) + 48, &heads[0], true) ||
        !u32(rt, qh(u, ep * 2) + 8, &heads[1], true))
        return false;
    heads[1] &= ~UINT32_C(0x1f);
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t dtd = heads[i];
        for (unsigned guard = 0; dtd && dtd != 1 && guard < 16; ++guard) {
            uint32_t token, next;
            if (dtd & 0x1f)
                break;
            if (!u32(rt, dtd + 4, &token, true))
                return false;
            if (token & ACTIVE) {
                *active = dtd;
                return true;
            }
            if (!u32(rt, dtd, &next, true))
                return false;
            if (next & 1)
                break;
            dtd = next & ~UINT32_C(0x1f);
        }
    }
    return true;
}

static void complete_out(m8rt *rt, unsigned ep)
{
    m8rt_usb *u = rt->usb;
    /* EP5 OUT is serviced only by the opt-in periodic audio host. */
    if (ep == 5)
        return;
    uint32_t dtd;
    if (!find_active(rt, ep, &dtd))
        return;
    if (!dtd) {
        if (ep < 2) {
            REG(u, ENDPTCOMPLETE) |= UINT32_C(1) << ep;
            raise_ui(rt, ep);
        }
        return;
    }
    uint32_t token, buffer;
    if (!w32(rt, qh(u, ep * 2) + 8, dtd, true) ||
        !u32(rt, dtd + 4, &token, true) ||
        !u32(rt, dtd + 8, &buffer, true))
        return;
    uint32_t capacity = (token >> 16) & 0x7fff;
    size_t count = 0, available = u->out.len - u->out.head;
    if (ep == 3 && available && buffer && capacity) {
        count = available < capacity ? available : capacity;
        const uint8_t *chunk = u->out.data + u->out.head;
        /* Consume before the write, as the reference does on a failed write. */
        u->out.head += count;
        if (m8rt_mem_write(rt, buffer, chunk, count)) {
            m8rt_fail(rt, "USB OUT payload write at %#x, %zu bytes", buffer, count);
            return;
        }
        if (u->out.head == u->out.len)
            u->out.head = u->out.len = 0;
    }
    if (ep == MIDI_EP && u->midi_in_count && buffer && capacity >= 4) {
        /* One USB packet, at most the firmware's full-speed 64-byte limit.
         * Do not split a four-byte event at the descriptor boundary. */
        unsigned events = capacity / 4;
        if (events > 16) events = 16;
        if (events > u->midi_in_count) events = u->midi_in_count;
        uint8_t payload[64];
        for (unsigned i = 0; i < events; ++i)
            memcpy(payload + i * 4, u->midi_in[(u->midi_in_head + i) % MIDI_CAP], 4);
        count = events * 4;
        if (m8rt_mem_write(rt, buffer, payload, count)) {
            m8rt_fail(rt, "USB MIDI OUT payload write at %#x, %zu bytes", buffer, count);
            return;
        }
        u->midi_in_head = (u->midi_in_head + events) % MIDI_CAP;
        u->midi_in_count -= events;
    }
    if (!count && ep >= 2)
        return;
    if (!w32(rt, dtd + 4, (token & ~ACTIVE & ~TOTAL_BYTES) |
             ((capacity - (uint32_t)count) << 16), true))
        return;
    REG(u, ENDPTCOMPLETE) |= UINT32_C(1) << ep;
    if (ep == MIDI_EP)
        REG(u, ENDPTSTAT) &= ~(UINT32_C(1) << ep);
    raise_ui(rt, ep);
}

static void receive_audio(m8rt *rt, uint32_t cycles)
{
    m8rt_usb *u = rt->usb;
    if (!u->audio_enabled)
        return;
    if (!u->boot_done || !u->config_injected || !u->qh_base ||
        !u->audio_interface_setting || (REG(u, ENDPTSETUPSTAT) & 1)) {
        u->audio_last_packet = cycles;
        return;
    }
    if ((uint32_t)(cycles - u->audio_last_packet) < USB_PERIOD)
        return;
    /* No host-time timer or opportunistic PRIME completion. A 250 us packet
     * carries 11 frames, with one extra frame every 40 packets (44100 Hz). */
    u->audio_last_packet += USB_PERIOD;
    uint32_t dtd, token, buffer;
    if (!find_active(rt, 5, &dtd) || !dtd)
        return;
    if (!u32(rt, dtd + 4, &token, true) || !u32(rt, dtd + 8, &buffer, true))
        return;
    uint32_t capacity = (token >> 16) & 0x7fff;
    uint32_t fraction = u->audio_fraction + 100;
    uint32_t frames = 11 + fraction / 4000;
    uint32_t bytes = frames * 4;
    if (capacity < bytes || !ram_buffer(buffer)) {
        m8rt_fail(rt, "USB audio OUT descriptor capacity=%u needs=%u buffer=%#x",
                  capacity, bytes, buffer);
        return;
    }
    uint8_t packet[48] = {0};
    uint64_t available = u->audio_written - u->audio_read;
    uint32_t take = available < frames ? (uint32_t)available : frames;
    for (uint32_t i = 0; i < take; ++i)
        memcpy(packet + i * 4, u->audio_input + ((u->audio_read + i) % AUDIO_INPUT_FRAMES) * 4, 4);
    /* Preserve partial effects on a failed guest write; never replay samples. */
    u->audio_read += take;
    u->audio_silence += frames - take;
    u->audio_fraction = fraction % 4000;
    if (m8rt_mem_write(rt, buffer, packet, bytes)) {
        m8rt_fail(rt, "USB audio OUT payload write at %#x, %u bytes", buffer, bytes);
        return;
    }
    if (!w32(rt, qh(u, 10) + 8, dtd, true) ||
        !w32(rt, dtd + 4, (token & ~ACTIVE & ~TOTAL_BYTES) | ((capacity - bytes) << 16), true))
        return;
    ++u->audio_packets;
    REG(u, ENDPTCOMPLETE) |= UINT32_C(1) << 5;
    REG(u, ENDPTSTAT) &= ~(UINT32_C(1) << 5);
    raise_ui(rt, 5);
}

static void inject_setup(m8rt *rt, uint32_t word1, uint32_t word2)
{
    m8rt_usb *u = rt->usb;
    if (!w32(rt, u->qh_base + 0x28, word1, true) ||
        !w32(rt, u->qh_base + 0x2c, word2, true))
        return;
    REG(u, ENDPTSETUPSTAT) = 1;
    REG(u, USBSTS) |= STS_UI;
    irq(rt);
}

static void poll_enum(m8rt *rt, uint32_t cycles)
{
    m8rt_usb *u = rt->usb;
    /* DTR is injected only after reset and configuration. No further setup
     * request is due until the host changes the audio interface setting.
     * Descriptor scans still run: guest RAM can change without PRIME. */
    if (u->dtr_injected && u->audio_interface_setting == u->audio_enabled)
        return;
    if (!u->running)
        return;
    if (!u->reset_injected && cycles >= ENUM_RESET_AT) {
        u->reset_injected = true;
        REG(u, PORTSC1) = 0x305;
        REG(u, USBSTS) |= STS_URI | STS_PCI;
        irq(rt);
    }
    if (u->reset_injected && !u->config_injected && u->qh_base &&
        !(REG(u, USBSTS) & STS_URI)) {
        u->config_injected = true;
        REG(u, PORTSC1) = 0x205;
        inject_setup(rt, UINT32_C(0x00010900), 0);
    }
    if (u->config_injected && !u->dtr_injected && u->qh_base &&
        !(REG(u, ENDPTSETUPSTAT) & 1)) {
        u->dtr_injected = true;
        inject_setup(rt, UINT32_C(0x00032221), 0);
    }
    if (u->dtr_injected && u->qh_base && !(REG(u, ENDPTSETUPSTAT) & 1) &&
        u->audio_interface_setting != u->audio_enabled) {
        /* Interface 4 is host-to-device audio in the pinned descriptors. */
        u->audio_interface_setting = u->audio_enabled;
        inject_setup(rt, u->audio_enabled ? UINT32_C(0x00010b01) : UINT32_C(0x00000b01), 4);
    }
}

int m8rt_usb_init(m8rt *rt)
{
    rt->usb = calloc(1, sizeof(*rt->usb));
    if (!rt->usb)
        return -1;
    REG(rt->usb, BURSTSIZE) = 0x0404;
    REG(rt->usb, ENDPTCTRL0) = 0x00800080;
    rt->usb->pcm_capture = true;
    return 0;
}

void m8rt_usb_destroy(m8rt *rt)
{
    if (rt->usb) {
        free(rt->usb->out.data);
        free(rt->usb);
        rt->usb = NULL;
    }
}

void m8rt_usb_reset(m8rt *rt)
{
    m8rt_usb *u = rt->usb;
    bool capture = u->pcm_capture, audio = u->audio_enabled, midi = u->midi_enabled;
    /* Retain host capture choices, queued audio source and completed MIDI.
     * Pending controller/MIDI inputs belong to the interrupted guest session. */
    memset(u->regs, 0, sizeof(u->regs));
    memset(u->written, 0, sizeof(u->written));
    u->qh_base = u->last_cyccnt = u->pcm_last_drain = u->gpt_arm_at = 0;
    u->insns_since_rs = u->in_bytes = u->pcm_bytes = 0;
    u->running = u->reset_injected = u->config_injected = u->dtr_injected = false;
    u->boot_done = u->gpt_armed = u->audio_interface_setting = false;
    u->out.len = u->out.head = 0;
    u->audio_last_packet = u->audio_fraction = 0;
    u->midi_in_head = u->midi_in_count = 0;
    u->pcm_capture = capture; u->audio_enabled = audio; u->midi_enabled = midi;
    REG(u, BURSTSIZE) = 0x0404;
    REG(u, ENDPTCTRL0) = 0x00800080;
}

uint32_t m8rt_usb_read(m8rt *rt, uint32_t addr)
{
    uint32_t offset = (addr & ~UINT32_C(3)) - USB1;
    if (offset >= 0x200 || offset == ENDPTPRIME || offset == ENDPTFLUSH)
        return 0;
    return REG(rt->usb, offset);
}

void m8rt_usb_write(m8rt *rt, uint32_t addr, uint32_t value)
{
    m8rt_usb *u = rt->usb;
    uint32_t offset = (addr & ~UINT32_C(3)) - USB1;
    if (offset >= 0x200)
        return;
    u->written[offset / 4] = true;
    switch (offset) {
    case USBCMD: {
        uint32_t current = REG(u, USBCMD);
        if (value & CMD_RST) {
            value &= ~CMD_RST;
            REG(u, USBMODE) = 0;
        }
        if (value & CMD_SUTW)
            current |= CMD_SUTW;
        else
            current &= ~CMD_SUTW;
        if (value & CMD_ATDTW)
            current |= CMD_ATDTW;
        if (value & CMD_RS) {
            current |= CMD_RS;
            u->running = true;
        }
        if (!(value & (CMD_RS | CMD_SUTW | CMD_ATDTW)))
            current = (current & ~CMD_RS) | (value & ~CMD_RST);
        REG(u, USBCMD) = current | (value & ~(CMD_RST | CMD_SUTW | CMD_ATDTW));
        return;
    }
    case USBSTS:
    case ENDPTSETUPSTAT:
    case ENDPTCOMPLETE:
        REG(u, offset) &= ~value;
        return;
    case ENDPTPRIME:
        if (u->qh_base) {
            for (unsigned ep = 0; ep < 8 && !rt->fatal; ++ep) {
                if (value & (UINT32_C(1) << (16 + ep)))
                    complete_in(rt, ep);
                if (!rt->fatal && (value & (UINT32_C(1) << ep)))
                    complete_out(rt, ep);
            }
            REG(u, ENDPTPRIME) = 0;
        }
        return;
    case ENDPTFLUSH:
        REG(u, ENDPTSTAT) &= ~value;
        return;
    case ENDPTLISTADDR:
        u->qh_base = value & ~UINT32_C(0x7ff);
        REG(u, offset) = u->qh_base;
        return;
    default:
        REG(u, offset) = value;
        return;
    }
}

void m8rt_usb_poll(m8rt *rt, uint32_t cycles)
{
    m8rt_usb *u = rt->usb;
    u->last_cyccnt = cycles;
    poll_enum(rt, cycles);
    if (rt->fatal)
        return;
    if (REG(u, GPTIMER0CTRL) & GPT_RUN) {
        if (REG(u, USBINTR) & INTR_TIE0) {
            if (!u->gpt_armed || cycles < u->gpt_arm_at) {
                u->gpt_arm_at = cycles;
                u->gpt_armed = true;
            }
            uint32_t load = u->written[GPTIMER0LD / 4] ? REG(u, GPTIMER0LD) : 74;
            uint64_t span = ((uint64_t)load + 1) * 600;
            if ((uint64_t)(cycles - u->gpt_arm_at) >= span) {
                REG(u, USBSTS) |= STS_TI0 | STS_UI;
                REG(u, GPTIMER0CTRL) &= ~GPT_RUN;
                u->gpt_armed = false;
                irq(rt);
            }
        }
    } else {
        u->gpt_armed = false;
    }
    drain_bulk(rt, 3);
    drain_bulk(rt, MIDI_EP);
    if (u->qh_base && u->midi_in_count && !rt->fatal)
        complete_out(rt, MIDI_EP);
    drain_pcm(rt, cycles);
    if (!rt->fatal)
        receive_audio(rt, cycles);
}

void m8rt_usb_tick_enum(m8rt *rt, uint32_t delta)
{
    if (rt->usb->running)
        rt->usb->insns_since_rs += delta;
}

void m8rt_usb_sof(m8rt *rt)
{
    if (REG(rt->usb, USBINTR) & INTR_SRE) {
        REG(rt->usb, USBSTS) |= STS_SRI;
        irq(rt);
    }
}

void m8rt_usb_set_boot(m8rt *rt)
{
    rt->usb->boot_done = true;
}

void m8rt_usb_set_capture(m8rt *rt, bool enabled)
{
    rt->usb->pcm_capture = enabled;
}

bool m8rt_usb_boot(m8rt *rt)
{
    return rt->usb->boot_done;
}

bool m8rt_usb_configured(m8rt *rt)
{
    return rt->usb->config_injected;
}

bool m8rt_usb_dtr(m8rt *rt)
{
    return rt->usb->dtr_injected;
}

size_t m8rt_usb_queued(m8rt *rt)
{
    return rt->usb->out.len - rt->usb->out.head;
}

int m8rt_usb_queue(m8rt *rt, const void *data, size_t count)
{
    if (m8rt_queue_append(&rt->usb->out, data, count))
        return -1;
    if (rt->usb->qh_base)
        complete_out(rt, 3);
    return rt->fatal ? -1 : 0;
}

int m8rt_set_audio_input(m8rt *rt, int enabled)
{
    if (!rt || !rt->usb || rt->fatal)
        return -1;
    m8rt_usb *u = rt->usb;
    if (enabled && !u->audio_enabled) {
        u->audio_last_packet = rt->cyccnt;
        u->audio_fraction = 0;
    }
    u->audio_enabled = enabled != 0;
    return 0;
}

int m8rt_queue_audio_input(m8rt *rt, const void *pcm, size_t bytes)
{
    if (!rt || !rt->usb || rt->fatal || bytes % 4 || (bytes && !pcm))
        return -1;
    m8rt_usb *u = rt->usb;
    size_t frames = bytes / 4;
    if (frames > AUDIO_INPUT_FRAMES - (u->audio_written - u->audio_read))
        return -2;
    for (size_t i = 0; i < frames; ++i)
        memcpy(u->audio_input + ((u->audio_written + i) % AUDIO_INPUT_FRAMES) * 4,
               (const uint8_t *)pcm + i * 4, 4);
    u->audio_written += frames;
    return frames ? m8rt_set_audio_input(rt, 1) : 0;
}

uint64_t m8rt_audio_input_get(m8rt *rt, unsigned field)
{
    if (!rt || !rt->usb)
        return 0;
    m8rt_usb *u = rt->usb;
    switch (field) {
    case 0: return u->audio_written - u->audio_read;
    case 1: return u->audio_written;
    case 2: return u->audio_read;
    case 3: return u->audio_silence;
    case 4: return u->audio_packets;
    case 5: return AUDIO_INPUT_FRAMES;
    default: return 0;
    }
}

bool m8rt_usb_audio_input_enabled(m8rt *rt)
{
    return rt->usb->audio_enabled;
}

uint32_t m8rt_usb_deadline(m8rt *rt)
{
    m8rt_usb *u = rt->usb;
    uint32_t now = rt->cyccnt, distance = USB_PERIOD;
    if (!u->boot_done || !u->config_injected || !u->dtr_injected ||
        m8rt_usb_queued(rt) || u->midi_in_count || (REG(u, 0x08c) & GPT_RUN))
        return 0;
    if ((REG(u, GPTIMER0CTRL) & GPT_RUN) && (REG(u, USBINTR) & INTR_TIE0)) {
        uint32_t load = u->written[GPTIMER0LD / 4] ? REG(u, GPTIMER0LD) : 74;
        uint64_t span = ((uint64_t)load + 1) * 600;
        if (!u->gpt_armed || now < u->gpt_arm_at || span >= UINT32_C(0x80000000))
            return 0;
        uint32_t elapsed = now - u->gpt_arm_at;
        uint32_t timer_distance = elapsed >= span ? 0 : (uint32_t)span - elapsed;
        if (timer_distance < distance)
            distance = timer_distance;
    }
    if (u->pcm_capture && u->qh_base) {
        uint32_t elapsed = now - u->pcm_last_drain;
        uint32_t pcm_distance = elapsed >= USB_PERIOD ? 0 : USB_PERIOD - elapsed;
        if (pcm_distance < distance)
            distance = pcm_distance;
    }
    if (u->audio_enabled && u->qh_base) {
        uint32_t elapsed = now - u->audio_last_packet;
        uint32_t input_distance = elapsed >= USB_PERIOD ? 0 : USB_PERIOD - elapsed;
        if (input_distance < distance)
            distance = input_distance;
    }
    return distance;
}

int m8rt_set_midi(m8rt *rt, int enabled)
{
    if (!rt || rt->fatal || rt->in_emu)
        return -1;
    m8rt_usb *u = rt->usb;
    u->midi_enabled = enabled != 0;
    if (!enabled) {
        u->midi_in_head = u->midi_in_count = 0;
        u->midi_out_head = u->midi_out_count = u->midi_read_offset = 0;
    }
    return 0;
}

int m8rt_midi_send(m8rt *rt, const void *data, size_t count)
{
    if (!rt || rt->fatal || rt->in_emu)
        return -1;
    if (!data || !count)
        return -2;
    const uint8_t *bytes = data;
    unsigned cin;
    size_t events = 1;
    uint8_t status = bytes[0];
    if (status == 0xf0) {
        if (count < 2 || count > MIDI_CAP * 3 || bytes[count - 1] != 0xf7)
            return -2;
        for (size_t i = 1; i + 1 < count; ++i)
            if (bytes[i] & 0x80) return -2;
        events = (count + 2) / 3;
        cin = 4;
    } else {
        if (status >= 0x80 && status < 0xf0) cin = status >> 4;
        else if (status == 0xf1 || status == 0xf3) cin = 2;
        else if (status == 0xf2) cin = 3;
        else if (status == 0xf6) cin = 5;
        else if (status >= 0xf8) cin = 15;
        else return -2;
        if (count != midi_length(cin)) return -2;
        for (size_t i = 1; i < count; ++i)
            if (bytes[i] & 0x80) return -2;
    }
    m8rt_usb *u = rt->usb;
    if (events > MIDI_CAP - u->midi_in_count)
        return -3;
    u->midi_enabled = true;
    for (size_t i = 0; i < count; i += 3) {
        size_t n = count - i;
        if (n > 3) n = 3;
        unsigned code = status == 0xf0 ? (i + n == count ? (unsigned)(4 + n) : 4) : cin;
        uint8_t *packet = u->midi_in[(u->midi_in_head + u->midi_in_count++) % MIDI_CAP];
        memset(packet, 0, 4);
        packet[0] = (uint8_t)code;
        memcpy(packet + 1, bytes + i, n);
    }
    /* Delivery is deferred to the next safe guest service boundary. */
    return 0;
}

size_t m8rt_midi_read_timed(m8rt *rt, void *out, size_t count, m8rt_midi_position *position)
{
    if (!rt || rt->in_emu || !out || count < 3 || !rt->usb->midi_out_count)
        return 0;
    m8rt_usb *u = rt->usb;
    midi_event *event = &u->midi_out[u->midi_out_head];
    unsigned n = event->count - u->midi_read_offset;
    memcpy(out, event->bytes + u->midi_read_offset, n);
    if (position) *position = event->position;
    u->midi_read_offset = 0;
    u->midi_out_head = (u->midi_out_head + 1) % MIDI_CAP;
    --u->midi_out_count;
    return n;
}

size_t m8rt_midi_read(m8rt *rt, void *out, size_t count)
{
    if (!rt || rt->in_emu || !out)
        return 0;
    m8rt_usb *u = rt->usb;
    size_t copied = 0;
    while (u->midi_out_count && copied < count) {
        midi_event *event = &u->midi_out[u->midi_out_head];
        size_t n = event->count - u->midi_read_offset;
        if (n > count - copied) n = count - copied;
        memcpy((uint8_t *)out + copied, event->bytes + u->midi_read_offset, n);
        u->midi_read_offset += (unsigned)n;
        copied += n;
        if (u->midi_read_offset == event->count) {
            u->midi_read_offset = 0;
            u->midi_out_head = (u->midi_out_head + 1) % MIDI_CAP;
            --u->midi_out_count;
        }
    }
    return copied;
}
