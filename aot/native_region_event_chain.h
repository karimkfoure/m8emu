#ifndef M8_NATIVE_REGION_EVENT_CHAIN_IMPL_H
#define M8_NATIVE_REGION_EVENT_CHAIN_IMPL_H
#include "native_events.h"

int m8_native_region_event_loop(void) { return 1; }

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
    uint64_t generation=*chain->generation;
    for(;;) {
        if(enter) {
            int action=events->event(events->user,M8_NATIVE_EVENT_ENTER,cpu->r[15],0);
            if(action==M8_NATIVE_EVENT_FAULT) return 1;
            if(action==M8_NATIVE_EVENT_EXIT || action==M8_NATIVE_EVENT_HANDOFF) return 0;
            if(action==M8_NATIVE_EVENT_AGAIN) continue;
            if(action!=M8_NATIVE_EVENT_RUN) return 1;
            events->segments++;enter=false;calls=0;edge=0;progressed=false;
            generation=*chain->generation;
        }
        uint32_t pc=cpu->r[15],reason=0;
        bool live_it=arch->itstate || m8_native_it_from_xpsr(cpu->xpsr);
        m8_native_chain_slot *slot=live_it?NULL:m8_native_chain_target(chain,pc,edge);
        *chain->mask_cache=chain->mask_seed;
        int result=2;
        bool aot_completed=false;
        if(slot && slot->aot) {
            result=slot->aot(cpu,mem);chain->aot_calls++;calls++;
            aot_completed=result!=M8_AOT_EXIT || cpu->r[15]!=pc;
            /* An unchanged-PC exit defers this instruction. Preserve any
             * published prefix and select the region/step fallback. */
            if(!aot_completed) result=2;
        }
        if(aot_completed) {
            if(progressed) chain->hops++;
            progressed=true;edge=0;
        } else if(slot && slot->region) {
            m8_native_region_exit exit={0};m8_fp_env fp_env;
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
                chain->region_calls++;chain->region_guest+=arch->last_executed;
                progressed=true;
            }
            reason=exit.reason;edge=exit.edge_id;
        }
        /* Preserve the original chain's prefix boundary before trying an
         * unknown/special entry. Completion can deliver a deferred SD IRQ. */
        if(result==2 && progressed) {
            reason=M8_NATIVE_EXIT_REQUIRED;result=0;
        } else if(result==2) {
            result=events->step(cpu,mem,arch);calls++;
            if(result==0) {
                events->fallback_steps++;
                events->fallback_guest+=arch->last_executed;
                events->interpreter_guest+=arch->last_special;
            }
            reason=M8_NATIVE_EXIT_REQUIRED;
        }
        if(result) {chain->exit_reason=result==1?M8_NATIVE_EXIT_FAULT:M8_NATIVE_EXIT_REQUIRED;return result;}
        bool service=*chain->service_exit || *chain->generation!=generation;
        bool required=reason && reason!=M8_NATIVE_EXIT_STATIC &&
            reason!=M8_NATIVE_EXIT_FALLTHROUGH && reason!=M8_NATIVE_EXIT_REDIRECT;
        if(service || required || m8_native_chain_boundary(cpu->r[15]) || calls>=chain->max_hops) {
            if(service) reason=M8_NATIVE_EXIT_MEMORY;
            else if(!required) reason=m8_native_chain_boundary(cpu->r[15])?
                M8_NATIVE_EXIT_REQUIRED:M8_NATIVE_EXIT_BUDGET;
            chain->exit_reason=reason;
            int action=events->event(events->user,M8_NATIVE_EVENT_COMPLETED,pc,reason);
            if(action==M8_NATIVE_EVENT_FAULT) return 1;
            if(action==M8_NATIVE_EVENT_EXIT || action==M8_NATIVE_EVENT_HANDOFF) return 0;
            if(action!=M8_NATIVE_EVENT_RUN && action!=M8_NATIVE_EVENT_AGAIN) return 1;
            enter=true;
        }
    }
}
#endif
