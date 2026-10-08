#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "mmio-state.h"

#define PERIPH_BASE UINT32_C(0x40000000)
#define PERIPH_SIZE UINT32_C(0x00400000)
#define GPIO_BASE UINT32_C(0x42000000)
#define GPIO_SIZE UINT32_C(0x00010000)
#define SCS_BASE UINT32_C(0xe000e000)
#define SCS_SIZE UINT32_C(0x1000)
#define ANALOG_BASE UINT32_C(0x400d8000)
#define ANALOG_END UINT32_C(0x400da000)
#define PLL_ARM ANALOG_BASE
#define PLL_USB1 (ANALOG_BASE + 0x10)
#define PLL_AUDIO (ANALOG_BASE + 0x70)
#define PLL_LOCK UINT32_C(0x80000000)
#define PLL_ENABLE UINT32_C(0x2000)
#define PLL_POWER UINT32_C(0x1000)
#define PLL_BYPASS UINT32_C(0x10000)
#define PLL_EN_USB_CLKS UINT32_C(0x40)
#define PLL_DIV_SELECT UINT32_C(0x2)
#define CCM UINT32_C(0x400fc000)
#define DCDC_REG0 UINT32_C(0x40080000)
#define FLEXSPI1 UINT32_C(0x402a8000)
#define FLEXSPI2 UINT32_C(0x402a4000)
#define USBPHY_CTRL UINT32_C(0x400d9030)
#define DEMCR UINT32_C(0xe000edfc)
#define CPACR UINT32_C(0xe000ed88)
#define VTOR UINT32_C(0xe000ed08)
#define SYST_CSR UINT32_C(0xe000e010)
#define SYST_RVR UINT32_C(0xe000e014)
#define SYST_CVR UINT32_C(0xe000e018)
#define NVIC_ISER UINT32_C(0xe000e100)
#define NVIC_ICER UINT32_C(0xe000e180)
#define NVIC_ISPR UINT32_C(0xe000e200)
#define NVIC_ICPR UINT32_C(0xe000e280)
#define NVIC_IPR UINT32_C(0xe000e400)
#define PIT UINT32_C(0x40084000)
#define SNVS UINT32_C(0x400d4000)
#define SNVS_HPCR (SNVS + 0x08)
#define SNVS_HPRTCMR (SNVS + 0x24)
#define SNVS_HPRTCLR (SNVS + 0x28)
#define SNVS_LPCR (SNVS + 0x38)
#define SNVS_LPSRTCMR (SNVS + 0x50)
#define SNVS_LPSRTCLR (SNVS + 0x54)
#define SNVS_HP_TS UINT32_C(0x10000)
#define RTC_MASK UINT64_C(0x7fffffffffff)
#define EDMA_BASE UINT32_C(0x400e8000)
#define EDMA_ERQ (EDMA_BASE + 0x0c)
#define EDMA_EEI (EDMA_BASE + 0x14)
#define EDMA_CEEI (EDMA_BASE + 0x18)
#define EDMA_SEEI (EDMA_BASE + 0x19)
#define EDMA_CERQ (EDMA_BASE + 0x1a)
#define EDMA_SERQ (EDMA_BASE + 0x1b)
#define EDMA_CDNE (EDMA_BASE + 0x1c)
#define EDMA_SSRT (EDMA_BASE + 0x1d)
#define EDMA_CERR (EDMA_BASE + 0x1e)
#define EDMA_CINT (EDMA_BASE + 0x1f)
#define EDMA_INT (EDMA_BASE + 0x24)
#define EDMA_ERR (EDMA_BASE + 0x2c)
#define TCD_BASE UINT32_C(0x400e9000)
#define TCD_END (TCD_BASE + 32 * 32)
#define DMAMUX_BASE UINT32_C(0x400ec000)
#define SAI_TCSR UINT32_C(0x40384008)
#define SAI_TDR0 UINT32_C(0x40384020)
#define CLOCK_HZ UINT64_C(600000000)
#define WORD_HZ UINT64_C(88200)

/* The dense banks cover the reference Machine's mapped device ranges. The
 * small sparse bank retains raw values used by host-side tests outside them. */
static uint32_t *raw_slot(m8rt_mmio *m, uint32_t address)
{
    address &= ~UINT32_C(3);
    if (address >= PERIPH_BASE && address - PERIPH_BASE < PERIPH_SIZE)
        return &m->periph[(address - PERIPH_BASE) / 4];
    if (address >= GPIO_BASE && address - GPIO_BASE < GPIO_SIZE)
        return &m->gpio[(address - GPIO_BASE) / 4];
    if (address >= SCS_BASE && address - SCS_BASE < SCS_SIZE)
        return &m->scs[(address - SCS_BASE) / 4];
    return NULL;
}

uint32_t m8rt_raw_read(m8rt *rt, uint32_t address)
{
    m8rt_mmio *m = rt->mmio;
    uint32_t *slot = raw_slot(m, address);
    if (slot) return *slot;
    address &= ~UINT32_C(3);
    for (size_t i = 0; i < m->extra_count; ++i)
        if (m->extra[i].address == address) return m->extra[i].value;
    return 0;
}

static void invalidate_route(m8rt_mmio *m, uint32_t address,
                             uint32_t old, uint32_t value)
{
    if (m->route_dirty || old == value) return;
    if (address == SAI_TCSR || address == EDMA_ERQ ||
        (address >= DMAMUX_BASE && address < DMAMUX_BASE + 16 * 4)) {
        m->route_dirty = true;
        return;
    }
    if (address >= TCD_BASE && address < TCD_BASE + 16 * 32) {
        switch ((address - TCD_BASE) % 32) {
        case 8: case 16:
            m->route_dirty = true;
            break;
        case 20:
            if (!(1 <= (old >> 16) && (old >> 16) <= 128 &&
                  1 <= (value >> 16) && (value >> 16) <= 128))
                m->route_dirty = true;
            break;
        case 28:
            if ((old >> 16) != (value >> 16)) m->route_dirty = true;
            break;
        default: break;
        }
    }
}

void m8rt_raw_write(m8rt *rt, uint32_t address, uint32_t value)
{
    m8rt_mmio *m = rt->mmio;
    address &= ~UINT32_C(3);
    uint32_t *slot = raw_slot(m, address);
    uint32_t old = slot ? *slot : m8rt_raw_read(rt, address);
    invalidate_route(m, address, old, value);
    if (slot) {
        *slot = value;
        if (address >= NVIC_IPR && address < NVIC_IPR + 256)
            rt->nvic_ipr[(address - NVIC_IPR) / 4] = value;
        return;
    }
    for (size_t i = 0; i < m->extra_count; ++i) {
        if (m->extra[i].address == address) {
            m->extra[i].value = value;
            return;
        }
    }
    if (m->extra_count == m->extra_cap) {
        size_t cap = m->extra_cap ? m->extra_cap * 2 : 16;
        raw_register *entries = realloc(m->extra, cap * sizeof(*entries));
        if (!entries) { m8rt_fail(rt, "allocate raw MMIO registers"); return; }
        m->extra = entries;
        m->extra_cap = cap;
    }
    m->extra[m->extra_count++] = (raw_register){ address, value };
}

int m8rt_mmio_init(m8rt *rt)
{
    m8rt_mmio *m = calloc(1, sizeof(*m));
    if (!m) return -1;
    m->periph = calloc(PERIPH_SIZE / 4, sizeof(uint32_t));
    if (!m->periph) { free(m); return -1; }
    rt->mmio = m;
    m->route = m->clock_route = -1;
    m->route_dirty = true;
    rt->syst_rvr = 99;
    m8rt_raw_write(rt, PLL_ARM, PLL_LOCK | PLL_ENABLE);
    m8rt_raw_write(rt, DCDC_REG0, UINT32_C(0x80000000));
    m8rt_raw_write(rt, FLEXSPI2 + 0x14, 1);
    return 0;
}

void m8rt_mmio_destroy(m8rt *rt)
{
    if (!rt->mmio) return;
    free(rt->mmio->periph);
    free(rt->mmio->extra);
    free(rt->mmio);
    rt->mmio = NULL;
}

/* A route lookup has no transfer effects. Cache only the register geometry;
 * live addresses, CITER, interrupt flags and clock credit remain uncached. */
static int resolve_route(m8rt *rt, bool fail)
{
    m8rt_mmio *m = rt->mmio;
    if (!m->route_dirty) return m->route;
    int route = -1;
    if ((m8rt_raw_read(rt, SAI_TCSR) & UINT32_C(0x80000001)) !=
        UINT32_C(0x80000001)) {
        m->route = -1; m->route_dirty = false; return -1;
    }
    uint32_t erq = m8rt_raw_read(rt, EDMA_ERQ) & 0xffff;
    for (unsigned ch = 0; ch < 16; ++ch) {
        if (!(erq & (UINT32_C(1) << ch))) continue;
        uint32_t base = TCD_BASE + ch * 32;
        uint32_t mux = m8rt_raw_read(rt, DMAMUX_BASE + ch * 4);
        if ((mux & UINT32_C(0x8000007f)) != UINT32_C(0x80000014) ||
            m8rt_raw_read(rt, base + 16) != SAI_TDR0) continue;
        uint32_t citer = m8rt_raw_read(rt, base + 20) >> 16;
        uint32_t biter = m8rt_raw_read(rt, base + 28) >> 16;
        if (!citer || !biter) continue;
        uint32_t nbytes = m8rt_raw_read(rt, base + 8);
        if (biter != 128 || citer > biter || nbytes != 4) {
            if (fail) m8rt_fail(rt,
                "SAI geometry: ch=%u BITER=%u CITER=%u NBYTES=%u", ch, biter, citer, nbytes);
            return -2;
        }
        if (route >= 0) {
            if (fail) m8rt_fail(rt, "SAI has multiple routes: first=%d next=%u ERQ=%08x",
                              route, ch, erq);
            return -2;
        }
        route = (int)ch;
    }
    m->route = route;
    m->route_dirty = false;
    return route;
}

int m8rt_sai_channel(m8rt *rt)
{
    return resolve_route(rt, true);
}

static int32_t signed16(uint32_t value)
{
    uint32_t bits = value & 0xffff;
    return bits >= 0x8000 ? -(int32_t)(0x10000 - bits) : (int32_t)bits;
}

static uc_err source_read(m8rt *rt, uint32_t address, unsigned count, uint32_t *value)
{
    const m8_region *regions[] = { &rt->memory.itcm, &rt->memory.dtcm,
                                 &rt->memory.ocram, &rt->memory.flash };
    const uint8_t *bytes = NULL;
    uint8_t scratch[4] = {0};
    for (unsigned i = 0; i < 4; ++i) {
        const m8_region *region = regions[i];
        if (region->host && address >= region->base && count <= region->size &&
            address - region->base <= region->size - count) {
            bytes = region->host + (address - region->base);
            break;
        }
    }
    if (!bytes) {
        uc_err err = (uc_err)m8rt_mem_read(rt, address, scratch, count);
        if (err != UC_ERR_OK) return err;
        bytes = scratch;
    }
    *value = count == 4 ? m8rt_le32(bytes) :
             count == 2 ? m8rt_le16(bytes) : bytes[0];
    return UC_ERR_OK;
}

void m8rt_sai_service(m8rt *rt)
{
    m8rt_mmio *m = rt->mmio;
    int route = resolve_route(rt, true);
    if (route < -1 || rt->fatal) return;
    uint32_t now = rt->cyccnt;
    if (route != m->clock_route || route < 0) {
        m->clock_route = route;
        m->clock_last = route < 0 ? 0 : now;
        m->fraction = m->pending = 0;
        return;
    }
    uint32_t elapsed = now - m->clock_last;
    /* Repeated MMIO at this clock has no new word credit. Pending words
     * still drain, and route validation above retains every fault/reset. */
    if (!elapsed && !m->pending) return;
    if (elapsed >= UINT32_C(0x80000000)) {
        m8rt_fail(rt, "ambiguous SAI clock interval: now=%u last=%u delta=%u",
                  now, m->clock_last, elapsed);
        return;
    }
    uint64_t total = m->fraction + (uint64_t)elapsed * WORD_HZ;
    m->pending += total / CLOCK_HZ;
    m->fraction = total % CLOCK_HZ;
    m->clock_last = now;
    unsigned budget = m->pending > 64 ? 64 : (unsigned)m->pending;
    if (!budget) return;
    unsigned moved = 0;
    /* Preserve the reference's ascending channel traversal and descriptor
     * snapshot at each channel entry, including its mid-major write order. */
    for (unsigned ch = 0; ch < 32 && budget; ++ch) {
        if (!(m8rt_raw_read(rt, EDMA_ERQ) & (UINT32_C(1) << ch))) continue;
        uint32_t base = TCD_BASE + ch * 32;
        uint32_t w4 = m8rt_raw_read(rt, base + 4);
        uint32_t w20 = m8rt_raw_read(rt, base + 20);
        uint32_t w28 = m8rt_raw_read(rt, base + 28);
        unsigned citer = w20 >> 16, biter = w28 >> 16;
        if ((citer | biter) & 0x8000) continue;
        citer &= 0x7fff; biter &= 0x7fff;
        if (!citer || !biter) continue;
        uint32_t mux = m8rt_raw_read(rt, DMAMUX_BASE + ch * 4);
        if (!(mux & UINT32_C(0x80000000)) || (mux & 0x7f) != 20 ||
            m8rt_raw_read(rt, base + 16) != SAI_TDR0) continue;
        unsigned nbytes = m8rt_raw_read(rt, base + 8);
        if (nbytes != 1 && nbytes != 2 && nbytes != 4) {
            m->pending -= moved;
            m8rt_fail(rt, "SAI transfer: ch=%u NBYTES=%u moved=%u", ch, nbytes, moved);
            return;
        }
        uint32_t saddr = m8rt_raw_read(rt, base);
        int32_t soff = signed16(w4), doff = signed16(w20);
        uint32_t slast = m8rt_raw_read(rt, base + 12);
        uint32_t dlast = m8rt_raw_read(rt, base + 24);
        unsigned csr = w28 & 0xffff;
        while (budget && citer) {
            uint32_t word;
            uc_err error = source_read(rt, saddr, nbytes, &word);
            if (error != UC_ERR_OK) {
                /* Prior words and descriptor progress stay retired. */
                m8rt_raw_write(rt, base + 20,
                    (m8rt_raw_read(rt, base + 20) & 0xffff) | (citer << 16));
                m->pending -= moved;
                m8rt_fail(rt, "SAI source read: ch=%u SADDR=%08x NBYTES=%u moved=%u error=%s",
                          ch, saddr, nbytes, moved, uc_strerror(error));
                return;
            }
            m8rt_raw_write(rt, SAI_TDR0, word);
            ++moved;
            saddr += (uint32_t)soff;
            m8rt_raw_write(rt, base, saddr);
            if (doff) m8rt_raw_write(rt, base + 16,
                m8rt_raw_read(rt, base + 16) + (uint32_t)doff);
            --citer;
            --budget;
            if ((csr & 4) && citer == biter / 2) {
                m8rt_raw_write(rt, EDMA_INT, m8rt_raw_read(rt, EDMA_INT) | (UINT32_C(1) << ch));
                m8rt_pend(rt, ch);
            }
            if (!citer) {
                saddr += slast;
                m8rt_raw_write(rt, base, saddr);
                m8rt_raw_write(rt, base + 16, m8rt_raw_read(rt, base + 16) + dlast);
                m8rt_raw_write(rt, base + 28, m8rt_raw_read(rt, base + 28) | 0x80);
                if (csr & 8) {
                    m8rt_raw_write(rt, EDMA_ERQ,
                        m8rt_raw_read(rt, EDMA_ERQ) & ~(UINT32_C(1) << ch));
                    m8rt_raw_write(rt, base + 20, m8rt_raw_read(rt, base + 20) & 0xffff);
                } else {
                    citer = biter;
                    m8rt_raw_write(rt, base + 20,
                        (m8rt_raw_read(rt, base + 20) & 0xffff) | (citer << 16));
                }
                if (csr & 2) {
                    m8rt_raw_write(rt, EDMA_INT, m8rt_raw_read(rt, EDMA_INT) | (UINT32_C(1) << ch));
                    m8rt_pend(rt, ch);
                }
            }
        }
        if (citer) m8rt_raw_write(rt, base + 20,
            (m8rt_raw_read(rt, base + 20) & 0xffff) | (citer << 16));
    }
    m->pending -= moved;
    if (budget) m8rt_fail(rt, "SAI elapsed-time transfer incomplete: moved=%u expected=%u route=%d",
                          moved, moved + budget, route);
}

uint32_t m8rt_sai_deadline(m8rt *rt)
{
    m8rt_mmio *m = rt->mmio;
    int route = resolve_route(rt, false);
    uint32_t erq = m8rt_raw_read(rt, EDMA_ERQ);
    if (route < -1 || erq != (route < 0 ? 0 : UINT32_C(1) << (unsigned)route)) return 0;
    if (route != m->clock_route || m->pending) return 0;
    if (route < 0) return (m->fraction || m->clock_last) ? 0 : UINT32_MAX;
    uint32_t elapsed = rt->cyccnt - m->clock_last;
    if (elapsed >= UINT32_C(0x80000000)) return 0;
    uint64_t total = m->fraction + (uint64_t)elapsed * WORD_HZ;
    if (total >= CLOCK_HZ) return 0;
    uint64_t bound = (64 * CLOCK_HZ - 1 - total) / WORD_HZ;
    uint32_t base = TCD_BASE + (unsigned)route * 32;
    unsigned citer = m8rt_raw_read(rt, base + 20) >> 16;
    unsigned csr = m8rt_raw_read(rt, base + 28) & 0xffff;
    unsigned until = (csr & 2) ? citer : 0;
    if ((csr & 4) && citer > 64 && (!until || citer - 64 < until)) until = citer - 64;
    if (until) {
        uint64_t distance = ((uint64_t)until * CLOCK_HZ - total + WORD_HZ - 1) / WORD_HZ;
        if (distance < bound) bound = distance;
    }
    return (uint32_t)bound;
}

/* Functional RTC contract used by Teensy startup.c and rtc.c: 47-bit counters
 * at 32768 Hz, stopped counter writes, independent enable bits, and HP_TS
 * synchronization. The epoch is entirely supplied by firmware. No wall clock,
 * alarm, security event, calibration, or power-off retention is modeled. */
static bool snvs_register(uint32_t address)
{
    return address == SNVS_HPCR || address == SNVS_HPRTCMR || address == SNVS_HPRTCLR ||
           address == SNVS_LPCR || address == SNVS_LPSRTCMR || address == SNVS_LPSRTCLR;
}

static void snvs_update(m8rt *rt)
{
    m8rt_mmio *m = rt->mmio;
    uint64_t elapsed = rt->guest_cycles - m->rtc_last_cycles;
    m->rtc_last_cycles = rt->guest_cycles;
    const uint32_t controls[] = {SNVS_HPCR, SNVS_LPCR};
    const uint32_t high[] = {SNVS_HPRTCMR, SNVS_LPSRTCMR};
    const uint32_t low[] = {SNVS_HPRTCLR, SNVS_LPSRTCLR};
    for (unsigned domain = 0; domain < 2; ++domain) {
        if (!(m8rt_raw_read(rt, controls[domain]) & 1)) continue;
        uint64_t fraction = m->rtc_fraction[domain] + (elapsed % CLOCK_HZ) * 32768;
        uint64_t ticks = (elapsed / CLOCK_HZ) * 32768 + fraction / CLOCK_HZ;
        m->rtc_fraction[domain] = fraction % CLOCK_HZ;
        uint64_t counter = (uint64_t)(m8rt_raw_read(rt, high[domain]) & 0x7fff) << 32 |
                           m8rt_raw_read(rt, low[domain]);
        counter = (counter + ticks) & RTC_MASK;
        m8rt_raw_write(rt, high[domain], (uint32_t)(counter >> 32));
        m8rt_raw_write(rt, low[domain], (uint32_t)counter);
    }
}

static void snvs_write(m8rt *rt, uint32_t address, uint32_t value)
{
    snvs_update(rt); /* Settle the old enabled state before a control changes. */
    if (address == SNVS_HPRTCMR || address == SNVS_LPSRTCMR) value &= 0x7fff;
    m8rt_raw_write(rt, address, value);
    if (address == SNVS_HPRTCMR || address == SNVS_HPRTCLR) rt->mmio->rtc_fraction[0] = 0;
    if (address == SNVS_LPSRTCMR || address == SNVS_LPSRTCLR) rt->mmio->rtc_fraction[1] = 0;
    if (address == SNVS_HPCR && (value & SNVS_HP_TS)) {
        m8rt_raw_write(rt, SNVS_HPRTCMR, m8rt_raw_read(rt, SNVS_LPSRTCMR) & 0x7fff);
        m8rt_raw_write(rt, SNVS_HPRTCLR, m8rt_raw_read(rt, SNVS_LPSRTCLR));
        rt->mmio->rtc_fraction[0] = rt->mmio->rtc_fraction[1];
    }
}

enum { SCS_RAW,SCS_ISER,SCS_ICER,SCS_ISPR,SCS_ICPR,SCS_VTOR,
       SCS_CSR,SCS_RVR,SCS_CVR,SCS_PLAIN };
#define SCS_BANKS(offset,kind) \
    [(offset)/4]=kind,[(offset)/4+1]=kind,[(offset)/4+2]=kind,[(offset)/4+3]=kind, \
    [(offset)/4+4]=kind,[(offset)/4+5]=kind,[(offset)/4+6]=kind,[(offset)/4+7]=kind
static const uint8_t scs_routes[SCS_SIZE/4]={
    SCS_BANKS(0x100,SCS_ISER),SCS_BANKS(0x180,SCS_ICER),
    SCS_BANKS(0x200,SCS_ISPR),SCS_BANKS(0x280,SCS_ICPR),
    [0xd08/4]=SCS_VTOR,[0x10/4]=SCS_CSR,[0x14/4]=SCS_RVR,[0x18/4]=SCS_CVR,
    [0xdfc/4]=SCS_PLAIN,[0xd88/4]=SCS_PLAIN
};
#undef SCS_BANKS
static uint32_t scs_read(m8rt *rt,uint32_t address)
{
    unsigned bank=(address&31)/4;
    switch(scs_routes[(address-SCS_BASE)/4]) {
    case SCS_ISER: return rt->nvic_iser[bank];
    case SCS_ISPR: case SCS_ICPR: return rt->nvic_ispr[bank];
    case SCS_VTOR: return rt->vtor;
    case SCS_CSR: return rt->syst_csr | UINT32_C(0x10000);
    case SCS_RVR: return rt->syst_rvr;
    case SCS_CVR: return rt->syst_cvr;
    default: return rt->mmio->scs[(address-SCS_BASE)/4]; /* Includes ICER reads. */
    }
}
static void scs_write(m8rt *rt,uint32_t address,uint32_t value)
{
    unsigned bank=(address&31)/4;
    switch(scs_routes[(address-SCS_BASE)/4]) {
    case SCS_ISER: rt->nvic_iser[bank]|=value;return;
    case SCS_ICER: rt->nvic_iser[bank]&=~value;return;
    case SCS_ISPR: m8rt_nvic_pending(rt,bank,rt->nvic_ispr[bank]|value);return;
    case SCS_ICPR: m8rt_nvic_pending(rt,bank,rt->nvic_ispr[bank]&~value);return;
    case SCS_VTOR: rt->vtor=value;return;
    case SCS_CSR: rt->syst_csr=value;return;
    case SCS_RVR: rt->syst_rvr=value;return;
    case SCS_CVR: rt->syst_cvr=0;return;
    case SCS_PLAIN: m8rt_raw_write(rt,address,value);return;
    default:
        m8rt_raw_write(rt,address,value);
        m8rt_sai_service(rt);
    }
}
uint32_t m8rt_mmio_read(m8rt *rt, uint32_t address)
{
    if (m8rt_raw_read(rt, EDMA_ERQ)) m8rt_sai_service(rt);
    uint32_t a = address & ~UINT32_C(3);
    /* Preserve SAI service before a read. SCS cannot overlap NOR or RTC. */
    if (a-SCS_BASE<SCS_SIZE) return scs_read(rt,a);
    uint32_t nor_value;
    if (m8rt_nor_read(rt, address, &nor_value)) return nor_value;
    if (snvs_register(a)) { snvs_update(rt); return m8rt_raw_read(rt, a); }
    if (a >= ANALOG_BASE && a < ANALOG_END && (a & 15)) a &= ~UINT32_C(15);
    uint32_t value = m8rt_raw_read(rt, a);
    if (a >= ANALOG_BASE && a < ANALOG_BASE + 0x80 && !(a & 15)) {
        if (a == PLL_USB1)
            value = (value | PLL_LOCK | PLL_ENABLE | PLL_POWER | PLL_EN_USB_CLKS) &
                    ~PLL_BYPASS & ~PLL_DIV_SELECT;
        else value |= PLL_LOCK | PLL_ENABLE;
        m8rt_raw_write(rt, a, value);
        return value;
    }
    if (a == CCM + 0x48) return 0;
    if (a >= CCM + 0x68 && a <= CCM + 0x84) return UINT32_MAX;
    if (a == DCDC_REG0) return value | UINT32_C(0x80000000);
    if (a == FLEXSPI1 || a == FLEXSPI2) return value & ~UINT32_C(1);
    if (a == FLEXSPI1 + 0x14 || a == FLEXSPI2 + 0x14) return value | 1;
    if (a == FLEXSPI1 + 0x100 || a == FLEXSPI2 + 0x100) return 0;
    if (a == USBPHY_CTRL) return value & ~UINT32_C(0xc0000000);
    if (a >= UINT32_C(0x400c4000) && a < UINT32_C(0x400c9000)) {
        unsigned off = a & 255;
        if (off == 8 || off == 0x20 || off == 0x28) return value | 1;
        if (off == 0x24 || off == 0x48) return value & ~UINT32_C(0x80);
        return value;
    }
    if (a >= UINT32_C(0x400d8180) && a < UINT32_C(0x400d81c0)) return 4;
    return value;
}

static bool edma_write(m8rt *rt, uint32_t address, uint32_t value)
{
    if (address >= EDMA_CEEI && address <= EDMA_CINT) {
        uint32_t a = address & ~UINT32_C(3);
        unsigned shift = (address & 3) * 8;
        uint32_t byte = value & 255;
        m8rt_raw_write(rt, a, (m8rt_raw_read(rt, a) & ~(UINT32_C(255) << shift)) | (byte << shift));
        if (address == EDMA_SSRT) {
            /* Fixed-clock mode rejects software requests before transfer. */
            m8rt_fail(rt, "software DMA request is unsupported by sai-fixed-44100-v1");
            return true;
        }
        if (byte & 128) return true;
        uint32_t target = 0;
        bool set = false;
        switch (address) {
        case EDMA_SERQ: target = EDMA_ERQ; set = true; break;
        case EDMA_CERQ: target = EDMA_ERQ; break;
        case EDMA_SEEI: target = EDMA_EEI; set = true; break;
        case EDMA_CEEI: target = EDMA_EEI; break;
        case EDMA_CINT: target = EDMA_INT; break;
        case EDMA_CERR: target = EDMA_ERR; break;
        case EDMA_CDNE:
            if (byte & 64) {
                for (unsigned ch = 0; ch < 32; ++ch) {
                    uint32_t tcd = TCD_BASE + ch * 32 + 28;
                    m8rt_raw_write(rt, tcd, m8rt_raw_read(rt, tcd) & ~UINT32_C(0x80));
                }
            } else {
                uint32_t tcd = TCD_BASE + (byte & 31) * 32 + 28;
                m8rt_raw_write(rt, tcd, m8rt_raw_read(rt, tcd) & ~UINT32_C(0x80));
            }
            return true;
        default: return true;
        }
        uint32_t old = m8rt_raw_read(rt, target);
        uint32_t bit = UINT32_C(1) << (byte & 31);
        m8rt_raw_write(rt, target, byte & 64 ? (set ? UINT32_MAX : 0) :
                        (set ? old | bit : old & ~bit));
        return true;
    }
    if (address >= TCD_BASE && address < TCD_END) {
        uint32_t a = address & ~UINT32_C(3);
        unsigned lane = address & 3, off = (a - TCD_BASE) % 32;
        uint32_t old = m8rt_raw_read(rt, a), merged = value;
        if (off == 4 || off == 20 || off == 28) {
            if (lane == 2) merged = (old & 0xffff) | ((value & 0xffff) << 16);
            else if (!lane && value <= 0xffff) merged = (old & UINT32_C(0xffff0000)) | value;
            else if (lane == 1 || lane == 3)
                merged = (old & ~(UINT32_C(255) << (lane * 8))) | ((value & 255) << (lane * 8));
        } else if (lane == 2 && value <= 0xffff) merged = (old & 0xffff) | (value << 16);
        else if (lane == 1 || lane == 3)
            merged = (old & ~(UINT32_C(255) << (lane * 8))) | ((value & 255) << (lane * 8));
        m8rt_raw_write(rt, a, merged);
        return true;
    }
    return false;
}

void m8rt_mmio_write(m8rt *rt, uint32_t address, uint32_t value)
{
    uint32_t a = address & ~UINT32_C(3);
    if (a-SCS_BASE<SCS_SIZE) { scs_write(rt,a,value);return; }
    if (m8rt_nor_write(rt, address, value)) return;
    if (snvs_register(a)) { snvs_write(rt, a, value); return; }
    if (a >= PIT + 0x10c && a <= PIT + 0x13c && (a - PIT - 0x10c) % 16 == 0) {
        m8rt_raw_write(rt, a, m8rt_raw_read(rt, a) & ~value);
        return;
    }
    if (a >= ANALOG_BASE && a < ANALOG_END) {
        uint32_t base = a & ~UINT32_C(15), old = m8rt_raw_read(rt, base);
        switch (a & 15) {
        case 4: old |= value; break;
        case 8: old &= ~value; break;
        case 12: old ^= value; break;
        default: old = value; break;
        }
        m8rt_raw_write(rt, base, old);
        return;
    }
    if (a == FLEXSPI2 + 0xb0) {
        m8rt_raw_write(rt, FLEXSPI2 + 0x14, m8rt_raw_read(rt, FLEXSPI2 + 0x14) | 1);
        m8rt_raw_write(rt, a, value);
        return;
    }
    if (a == FLEXSPI2) { m8rt_raw_write(rt, a, value & ~UINT32_C(1)); return; }
    /* USBPHY_CTRL is in the alias range above, as in the Python reference. */
    if (a == USBPHY_CTRL) { m8rt_raw_write(rt, a, value & ~UINT32_C(0x80000000)); return; }
    if (edma_write(rt, address, value)) { if (!rt->fatal) m8rt_sai_service(rt); return; }
    m8rt_raw_write(rt, a, value);
    m8rt_sai_service(rt);
}

/* Classify only behavior implemented here, including registers consumed by the
 * asynchronous SAI/eDMA model. The remaining register file is not a model.
 * Direction matters: an ADC status-bit read does not model its configuration writes. */
bool m8rt_mmio_modeled(uint32_t address, bool write)
{
    uint32_t a = address & ~UINT32_C(3);
    if (a == FLEXSPI1 || a == FLEXSPI1 + 0x14 ||
        a == FLEXSPI1 + 0x18 || a == FLEXSPI1 + 0x1c ||
        a == FLEXSPI1 + 0xa0 || a == FLEXSPI1 + 0xa4 ||
        a == FLEXSPI1 + 0xb0 || a == FLEXSPI1 + 0xb8 || a == FLEXSPI1 + 0xbc ||
        a == FLEXSPI1 + 0x100 || (a >= FLEXSPI1 + 0x180 && a < FLEXSPI1 + 0x188) ||
        (a >= FLEXSPI1 + 0x200 && a < FLEXSPI1 + 0x300)) return true;
    if (snvs_register(a)) return true;
    if ((a >= NVIC_ISER && a < NVIC_ISER + 32) ||
        (a >= NVIC_ISPR && a < NVIC_ISPR + 32) ||
        (a >= NVIC_ICPR && a < NVIC_ICPR + 32) ||
        a == VTOR || a == SYST_CSR || a == SYST_RVR || a == SYST_CVR)
        return true;
    if (a == EDMA_ERQ || a == EDMA_INT || a == EDMA_EEI || a == EDMA_ERR ||
        (a >= TCD_BASE && a < TCD_END) ||
        (a >= DMAMUX_BASE && a < DMAMUX_BASE + 32 * 4) || a == SAI_TCSR)
        return true;
    if (write) {
        if ((a >= NVIC_ICER && a < NVIC_ICER + 32) ||
            (a >= NVIC_IPR && a < NVIC_IPR + 256) ||
            a == DEMCR || a == CPACR ||
            (a >= ANALOG_BASE && a < ANALOG_END) ||
            (address >= EDMA_CEEI && address <= EDMA_CINT) ||
            a == FLEXSPI2 || a == FLEXSPI2 + 0xb0)
            return true;
        return a >= PIT + 0x10c && a <= PIT + 0x13c &&
               (a - PIT - 0x10c) % 16 == 0;
    }
    if (a >= ANALOG_BASE && a < ANALOG_END && (a & 15)) a &= ~UINT32_C(15);
    if ((a >= ANALOG_BASE && a < ANALOG_BASE + 0x80 && !(a & 15)) ||
        a == CCM + 0x48 || (a >= CCM + 0x68 && a <= CCM + 0x84) ||
        a == DCDC_REG0 || a == FLEXSPI1 || a == FLEXSPI2 ||
        a == FLEXSPI1 + 0x14 || a == FLEXSPI2 + 0x14 ||
        a == FLEXSPI1 + 0x100 || a == FLEXSPI2 + 0x100 || a == USBPHY_CTRL ||
        (a >= UINT32_C(0x400d8180) && a < UINT32_C(0x400d81c0)))
        return true;
    if (a >= UINT32_C(0x400c4000) && a < UINT32_C(0x400c9000)) {
        unsigned off = a & 255;
        return off == 8 || off == 0x20 || off == 0x28 || off == 0x24 || off == 0x48;
    }
    return false;
}

int m8rt_mmio_reset(m8rt *rt)
{
    /* The retained SNVS clock keeps the session's firmware epoch. Other
     * modeled registers and SAI/eDMA credit belong to the stopped CPU. */
    const uint32_t addresses[] = {SNVS_HPCR, SNVS_HPRTCMR, SNVS_HPRTCLR,
                                 SNVS_LPCR, SNVS_LPSRTCMR, SNVS_LPSRTCLR};
    uint32_t values[6];
    snvs_update(rt);
    for(unsigned i=0;i<6;i++) values[i]=m8rt_raw_read(rt,addresses[i]);
    uint64_t fractions[2]={rt->mmio->rtc_fraction[0],rt->mmio->rtc_fraction[1]};
    m8rt_mmio_destroy(rt);
    if(m8rt_mmio_init(rt)) return -1;
    for(unsigned i=0;i<6;i++) m8rt_raw_write(rt,addresses[i],values[i]);
    rt->mmio->rtc_fraction[0]=fractions[0];rt->mmio->rtc_fraction[1]=fractions[1];
    rt->mmio->rtc_last_cycles=rt->guest_cycles;
    return 0;
}
