#ifndef M8_NATIVE_REGION_CHAIN_IMPL_H
#define M8_NATIVE_REGION_CHAIN_IMPL_H
#include "native_regions.h"

#define M8_NATIVE_CHAIN_ENTRY_SLOTS 1024u

static inline bool m8_native_chain_boundary(uint32_t pc) {
    return (pc>=0x6005118cu && pc<=0x60051192u) ||
           (pc>=0x60005b98u && pc<=0x60005bacu) ||
           (pc>=0x3205au && pc<=0x3205cu) ||
           (pc>=0x6003b6b0u && pc<=0x6003b6b4u) ||
           pc==0x33f9cu || pc==0x33fdau || pc==0x21828u ||
           pc>=0xf0000000u;
}

/* The generated page resolver and static-edge count precede this include. */
size_t m8_native_region_chain_cache_slots(void) {
    return m8_native_region_chain_edge_count+M8_NATIVE_CHAIN_ENTRY_SLOTS;
}
/* Keep pure, infrequent PC resolution out of the dispatch loop. The caller
 * checks generation after every completed hop before admitting another one. */
static __attribute__((noinline)) void m8_native_chain_fill(m8_native_chain *chain,
        m8_native_chain_slot *slot,uint32_t pc,uint64_t generation) {
    chain->cache_misses++;
    slot->aot=chain->lookup(chain->user,pc);
    slot->region=m8_native_region_chain_lookup(pc);
    slot->has_fp=m8_native_region_chain_has_fp(pc);
    slot->generation=generation;slot->pc=pc;slot->valid=true;
}
static inline m8_native_chain_slot *m8_native_chain_target(m8_native_chain *chain,
        uint32_t pc,unsigned edge,uint64_t admitted_generation) {
    unsigned index=edge?edge:m8_native_region_chain_edge_count+
        (((pc>>1)^(pc>>11)^(pc>>21))&(M8_NATIVE_CHAIN_ENTRY_SLOTS-1u));
    if(index>=chain->slot_count) return NULL;
    m8_native_chain_slot *slot=&chain->slots[index];
    uint64_t generation=admitted_generation;
    if(slot->valid && slot->generation==generation && slot->pc==pc) {
        chain->cache_hits++;
    } else {
        m8_native_chain_fill(chain,slot,pc,generation);
    }
    return slot;
}

int m8_native_region_chain(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch,
                           m8_native_chain *chain) {
    unsigned edge=0, calls=0;
    bool progressed=false;
    if(!chain || !chain->generation || !chain->lookup || !chain->slots ||
       !chain->service_exit || !chain->mask_cache || !chain->max_hops) return 2;
    chain->exit_reason=0;
    uint64_t admitted_generation=*chain->generation;
    while(calls<chain->max_hops) {
        uint32_t pc=cpu->r[15];
        if(arch->itstate || m8_native_it_from_xpsr(cpu->xpsr)) {
            chain->exit_reason=M8_NATIVE_EXIT_REQUIRED;
            return progressed?0:2;
        }
        m8_native_chain_slot *slot=m8_native_chain_target(chain,pc,edge,admitted_generation);
        if(!slot) {chain->exit_reason=M8_NATIVE_EXIT_REQUIRED;return progressed?0:2;}
        *chain->mask_cache=chain->mask_seed;
        int result=2;
        bool aot_completed=false;
        if(slot->aot) {
            result=slot->aot(cpu,mem);chain->aot_calls++;calls++;
            aot_completed=result!=M8_AOT_EXIT || cpu->r[15]!=pc;
        }
        if(aot_completed) {
            if(progressed) chain->hops++;
            progressed=true;
            if(result) {chain->exit_reason=M8_NATIVE_EXIT_FAULT;return result;}
            if(*chain->service_exit || *chain->generation!=admitted_generation) {
                chain->exit_reason=M8_NATIVE_EXIT_MEMORY;return 0;
            }
            /* AOT does not export static exit descriptors. Resolve its next
             * known entry through the generation-checked entry cache. */
            edge=0;
        } else {
            if(!slot->region) {chain->exit_reason=M8_NATIVE_EXIT_REQUIRED;return progressed?0:2;}
            m8_native_region_exit exit={0};
            m8_fp_env fp_env;
#if M8_NATIVE_REGION_INTEGER_FP_FASTPATH
            if(slot->has_fp) fp_env=m8_fp_enter(cpu);
#else
            fp_env=m8_fp_enter(cpu);
#endif
            result=slot->region(cpu,mem,arch,&exit);calls++;
#if M8_NATIVE_REGION_INTEGER_FP_FASTPATH
            if(slot->has_fp) m8_fp_leave(fp_env);
#else
            m8_fp_leave(fp_env);
#endif
            if(result!=2) {
                if(progressed) chain->hops++;
                if(!arch->ipsr && (arch->control&2u)) arch->psp=cpu->r[13];
                else arch->msp=cpu->r[13];
                chain->region_calls++;
                chain->region_guest+=arch->last_executed;
                progressed=true;
            }
            if(result) {
                chain->exit_reason=result==1?M8_NATIVE_EXIT_FAULT:M8_NATIVE_EXIT_REQUIRED;
                return result==2 && progressed?0:result;
            }
            if(*chain->service_exit || *chain->generation!=admitted_generation ||
               (exit.reason!=M8_NATIVE_EXIT_STATIC && exit.reason!=M8_NATIVE_EXIT_FALLTHROUGH &&
                exit.reason!=M8_NATIVE_EXIT_REDIRECT)) {
                chain->exit_reason=exit.reason;return 0;
            }
            edge=exit.edge_id;
        }
        if(m8_native_chain_boundary(cpu->r[15])) {
            chain->exit_reason=M8_NATIVE_EXIT_REQUIRED;return 0;
        }
    }
    /* Keep host-stop polling bounded. This does not complete an engine slice
     * or add a guest time charge; the runtime resumes its inner dispatch. */
    chain->exit_reason=M8_NATIVE_EXIT_BUDGET;
    return 0;
}
#endif
