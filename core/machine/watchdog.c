#include "internal.h"
#include <string.h>

#define WDOG1 UINT32_C(0x400b8000)
#define WDE UINT16_C(0x0004)
#define WDT UINT16_C(0x0008)
#define SRS UINT16_C(0x0010)
#define WIE UINT16_C(0x8000)
#define WTIS UINT16_C(0x4000)
#define HALF_SECOND UINT64_C(300000000)
#define WDOG_IRQ 92

bool m8rt_watchdog_handles(uint32_t addr)
{
    return addr >= WDOG1 && addr < WDOG1 + 10;
}

void m8rt_watchdog_init(m8rt *rt, bool reset)
{
    m8rt_watchdog_status *w = &rt->watchdog;
    uint32_t retained = reset ? w->control & (WDT | UINT16_C(0x0040)) : 0;
    if (!reset) memset(w, 0, sizeof(*w));
    w->control = UINT16_C(0x0030) | retained;
    w->service = 0;
    w->reset_status = reset ? w->reset_status : UINT16_C(0x0010);
    w->interrupt_control = 4;
    w->miscellaneous = 1;
    w->enabled = w->reset_pending = 0;
    w->deadline_cycles = 0;
    rt->watchdog_service_armed = rt->watchdog_control_locked = false;
    rt->watchdog_interrupt_locked = rt->watchdog_warned = false;
}

static uint16_t read_register(m8rt *rt, unsigned offset)
{
    const m8rt_watchdog_status *w = &rt->watchdog;
    switch (offset) {
    case 0: return (uint16_t)w->control;
    case 2: return (uint16_t)w->service;
    case 4: return (uint16_t)w->reset_status;
    case 6: return (uint16_t)w->interrupt_control;
    case 8: return (uint16_t)w->miscellaneous;
    default: return 0;
    }
}

uint32_t m8rt_watchdog_read(m8rt *rt, uint32_t addr, unsigned size)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < size && i < 4; ++i) {
        unsigned off = addr + i - WDOG1;
        value |= (uint32_t)((read_register(rt, off & ~1u) >> (8 * (off & 1))) & 255) << (8 * i);
    }
    return value;
}

static void reload(m8rt *rt)
{
    m8rt_watchdog_status *w = &rt->watchdog;
    w->deadline_cycles = rt->guest_cycles + ((w->control >> 8) + 1) * HALF_SECOND;
    rt->watchdog_warned = false;
}

static void request_reset(m8rt *rt, uint32_t source)
{
    rt->watchdog.reset_status = source;
    rt->watchdog.reset_pending = 1;
    if (rt->in_emu) m8rt_engine_stop(rt);
}

static void write_register(m8rt *rt, unsigned off, uint16_t value, uint16_t mask)
{
    m8rt_watchdog_status *w = &rt->watchdog;
    uint16_t old = read_register(rt, off);
    value = (uint16_t)((old & ~mask) | (value & mask));
    switch (off) {
    case 0: {
        uint16_t locked = UINT16_C(0x0083); /* WDW, WDBG, WDZST */
        if (rt->watchdog_control_locked) value = (uint16_t)((value & ~locked) | (old & locked));
        rt->watchdog_control_locked = true;
        value |= old & (WDE | WDT);
        w->control = value;
        if (!(value & SRS)) request_reset(rt, 1);
        if (!(old & WDE) && (value & WDE)) { w->enabled = 1; reload(rt); }
        break;
    }
    case 2:
        w->service = value;
        if (mask != UINT16_MAX) { rt->watchdog_service_armed = false; ++w->rejected_services; break; }
        if (value == UINT16_C(0xaaaa) && rt->watchdog_service_armed) {
            rt->watchdog_service_armed = false;
            if (w->enabled && !w->reset_pending && rt->guest_cycles < w->deadline_cycles) {
                reload(rt); w->last_refresh_cycles = rt->guest_cycles; ++w->refreshes;
            }
        } else {
            if (value != UINT16_C(0x5555) || rt->watchdog_service_armed) ++w->rejected_services;
            rt->watchdog_service_armed = value == UINT16_C(0x5555);
        }
        break;
    case 4:
        m8rt_fail(rt, "WDOG1 WRSR write addr=%08x value=%04x mask=%04x", WDOG1 + off, value, mask);
        break;
    case 6: {
        uint16_t status = old & WTIS;
        if ((value & mask) & WTIS) status = 0;
        uint16_t config = rt->watchdog_interrupt_locked ? old & UINT16_C(0x80ff) : value & UINT16_C(0x80ff);
        rt->watchdog_interrupt_locked = true;
        w->interrupt_control = config | status;
        if (!status) m8rt_nvic_pending(rt,WDOG_IRQ >> 5,rt->nvic_ispr[WDOG_IRQ >> 5] & ~(1u << (WDOG_IRQ & 31)));
        break;
    }
    case 8:
        w->miscellaneous = old & value & 1;
        break;
    }
}

void m8rt_watchdog_write(m8rt *rt, uint32_t addr, unsigned size, uint32_t value)
{
    m8rt_watchdog_tick(rt);
    if (rt->watchdog.reset_pending) return;
    for (unsigned i = 0; i < size && i < 4;) {
        unsigned off = addr + i - WDOG1;
        uint16_t mask = (uint16_t)(255u << (8 * (off & 1)));
        uint16_t data = (uint16_t)(((value >> (8 * i)) & 255) << (8 * (off & 1)));
        ++i;
        if (!(off & 1) && i < size) { mask = UINT16_MAX; data |= (uint16_t)(((value >> (8 * i)) & 255) << 8); ++i; }
        write_register(rt, off & ~1u, data, mask);
        if (rt->watchdog.reset_pending || rt->fatal) return;
    }
}

void m8rt_watchdog_tick(m8rt *rt)
{
    m8rt_watchdog_status *w = &rt->watchdog;
    if (!w->enabled || w->reset_pending) return;
    uint64_t warning = (w->interrupt_control & 255) * HALF_SECOND;
    if ((w->interrupt_control & WIE) && !rt->watchdog_warned &&
        (rt->guest_cycles >= w->deadline_cycles || w->deadline_cycles - rt->guest_cycles <= warning)) {
        rt->watchdog_warned = true;
        w->interrupt_control |= WTIS;
        ++w->interrupts;
        m8rt_pend(rt, WDOG_IRQ);
    }
    if (rt->guest_cycles >= w->deadline_cycles) request_reset(rt, 2);
}

bool m8rt_watchdog_irq(m8rt *rt)
{
    return !rt->watchdog.reset_pending && (rt->watchdog.interrupt_control & (WIE | WTIS)) == (WIE | WTIS);
}

int m8rt_get_watchdog_status(m8rt *rt, m8rt_watchdog_status *out)
{
    if (!rt || !out || rt->in_emu) return -1;
    *out = rt->watchdog;
    return 0;
}
