#ifndef M8_NATIVE_REGION_EVENT_CHAIN_IMPL_H
#define M8_NATIVE_REGION_EVENT_CHAIN_IMPL_H
#include "native_events.h"

int m8_native_region_event_loop(void) { return 1; }

/* FPSCR mode bits that select host FP control (rounding + flush-to-zero).
 * While the FP environment is hoisted over consecutive region hops, host
 * exception flags OR-accumulate and host control stays guest-configured.
 * Every flag merge point either OR-merges (VMRS sync, segment leave) or
 * replaces and clears (VMSR), so guest FPSCR observations match per-region
 * enter/leave exactly; control is reconfigured whenever the mode bits move
 * (VMSR or an interleaved AOT hop), which VMSR already pairs with its own
 * FPSCR replace, flag clear and reconfigure. */
#define M8_NATIVE_CHAIN_FP_MODE_MASK ((3u<<22)|(1u<<24))

/* The original cache resolver and region implementations precede this file.
 * Exactly the old inner-dispatch boundaries invoke event completion/entry.
 * Static compiled transfers have no new clock or interrupt observation. */
int m8_native_region_event_chain(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch,
                                 m8_native_chain *chain,m8_native_events *events) {
    if(!chain || !events || !events->event || !events->step ||
       !chain->generation || !chain->lookup || !chain->slots ||
       !chain->service_exit || !chain->mask_cache || !chain->max_hops) return 2;
    unsigned edge=0,calls=0;
    bool enter=true,progressed=false;
    /* Hoisted FP environment: at most one enter/leave pair per segment.
     * Integer regions never touch host FP state, so keeping the guest
     * configuration across them changes no guest-visible effect. */
    bool fp_active=false;
    m8_fp_env fp_env={0};
    uint32_t fp_mode=0;
    uint64_t generation=*chain->generation;
    /* Loop invariants: the runtime owns these for the complete bounded call,
     * so limit/seed/pointer reloads leave the hop loop. Statistics accumulate
     * in locals: the runtime reads chain_state only after a return
     * (native-engine accumulates it into stats there), events->step touches
     * only the events counters, and the ENTER handler resets service_exit
     * through the same pointer, so flushing at each return preserves every
     * observed count and flag exactly. */
    const unsigned max_hops=chain->max_hops;
    const unsigned mask_seed=chain->mask_seed;
    const bool *service_exit=chain->service_exit;
    const uint64_t *chain_generation=chain->generation;
    unsigned *mask_cache=chain->mask_cache;
    uint64_t aot_calls=0,region_calls=0,region_guest=0,hops=0;
#define M8_CHAIN_FLUSH_STATS do{chain->aot_calls+=aot_calls;chain->region_calls+=region_calls;chain->region_guest+=region_guest;chain->hops+=hops;}while(0)
    for(;;) {
        if(enter) {
            int action=events->event(events->user,M8_NATIVE_EVENT_ENTER,cpu->r[15],0);
            if(action==M8_NATIVE_EVENT_FAULT) {M8_CHAIN_FLUSH_STATS;return 1;}
            if(action==M8_NATIVE_EVENT_EXIT || action==M8_NATIVE_EVENT_HANDOFF) {M8_CHAIN_FLUSH_STATS;return 0;}
            if(action==M8_NATIVE_EVENT_AGAIN) continue;
            if(action!=M8_NATIVE_EVENT_RUN) {M8_CHAIN_FLUSH_STATS;return 1;}
            events->segments++;enter=false;calls=0;edge=0;progressed=false;
            generation=*chain_generation;
        }
        uint32_t pc=cpu->r[15],reason=0;
        bool live_it=arch->itstate || m8_native_it_from_xpsr(cpu->xpsr);
        m8_native_chain_slot *slot=live_it?NULL:m8_native_chain_target(chain,pc,edge,generation);
        *mask_cache=mask_seed;
        int result=2;
        bool aot_completed=false;
        if(slot && slot->aot) {
            result=slot->aot(cpu,mem);aot_calls++;calls++;
            aot_completed=result!=M8_AOT_EXIT || cpu->r[15]!=pc;
            /* An unchanged-PC exit defers this instruction. Preserve any
             * published prefix and select the region/step fallback. */
            if(!aot_completed) result=2;
        }
        if(aot_completed) {
            if(progressed) hops++;
            progressed=true;edge=0;
        } else if(slot && slot->region) {
            m8_native_region_exit exit={0};
#if M8_NATIVE_REGION_INTEGER_FP_FASTPATH
            bool need_fp=slot->has_fp;
#else
            bool need_fp=true;
#endif
            if(need_fp) {
                if(!fp_active) {
                    fp_env=m8_fp_enter(cpu);fp_active=true;
                    fp_mode=cpu->fpscr & M8_NATIVE_CHAIN_FP_MODE_MASK;
                } else if((cpu->fpscr ^ fp_mode) & M8_NATIVE_CHAIN_FP_MODE_MASK) {
                    m8_fp_configure(cpu);m8_fp_clear();
                    fp_mode=cpu->fpscr & M8_NATIVE_CHAIN_FP_MODE_MASK;
                }
            }
            result=slot->region(cpu,mem,arch,&exit);calls++;
            if(result!=2) {
                if(progressed) hops++;
                if(!arch->ipsr && (arch->control&2u)) arch->psp=cpu->r[13];
                else arch->msp=cpu->r[13];
                region_calls++;region_guest+=arch->last_executed;
                progressed=true;
            }
            reason=exit.reason;edge=exit.edge_id;
        }
        /* Preserve the original chain's prefix boundary before trying an
         * unknown/special entry. Completion can deliver a deferred SD IRQ. */
        if(result==2 && progressed) {
            reason=M8_NATIVE_EXIT_REQUIRED;result=0;
        } else if(result==2) {
            if(fp_active) {m8_fp_leave(fp_env);fp_active=false;}
            result=events->step(cpu,mem,arch);calls++;
            if(result==0) {
                events->fallback_steps++;
                events->fallback_guest+=arch->last_executed;
                events->interpreter_guest+=arch->last_special;
            }
            reason=M8_NATIVE_EXIT_REQUIRED;
        }
        if(result) {
            if(fp_active) {m8_fp_leave(fp_env);fp_active=false;}
            chain->exit_reason=result==1?M8_NATIVE_EXIT_FAULT:M8_NATIVE_EXIT_REQUIRED;M8_CHAIN_FLUSH_STATS;return result;}
        bool service=*service_exit || *chain_generation!=generation;
        bool required=reason && reason!=M8_NATIVE_EXIT_STATIC &&
            reason!=M8_NATIVE_EXIT_FALLTHROUGH && reason!=M8_NATIVE_EXIT_REDIRECT;
        /* m8_native_chain_boundary is pure in the new PC: the old code
         * evaluated it in the condition and again to select the
         * REQUIRED/BUDGET reason, with no PC write between. One evaluation
         * selects the identical reason; on the service/required paths the
         * call count is unchanged (zero, by short-circuit, as before). */
        bool over_budget=calls>=max_hops;
        bool hit_boundary=!service && !required && m8_native_chain_boundary(cpu->r[15]);
        if(service || required || hit_boundary || over_budget) {
            if(service) reason=M8_NATIVE_EXIT_MEMORY;
            else if(!required) reason=hit_boundary?
                M8_NATIVE_EXIT_REQUIRED:M8_NATIVE_EXIT_BUDGET;
            if(fp_active) {m8_fp_leave(fp_env);fp_active=false;}
            chain->exit_reason=reason;
            int action=events->event(events->user,M8_NATIVE_EVENT_COMPLETED,pc,reason);
            if(action==M8_NATIVE_EVENT_FAULT) {M8_CHAIN_FLUSH_STATS;return 1;}
            if(action==M8_NATIVE_EVENT_EXIT || action==M8_NATIVE_EVENT_HANDOFF) {M8_CHAIN_FLUSH_STATS;return 0;}
            if(action!=M8_NATIVE_EVENT_RUN && action!=M8_NATIVE_EVENT_AGAIN) {M8_CHAIN_FLUSH_STATS;return 1;}
            enter=true;
        }
    }
}
#undef M8_CHAIN_FLUSH_STATS
#endif
