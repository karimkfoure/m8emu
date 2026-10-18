/* Portable ARMv7E-M instruction fallback. No code cache or external decoder. */
#include "thumb-interpreter.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "native_step_special.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

typedef struct thumb {
    m8_cpu *c;
    m8_mem *m;
    m8_native_arch *a;
    uint32_t pc, next, w;
    unsigned width;
    int in_it, slow, special, fp_prepared;
    const char *family;
    uint32_t reason, address, size, write;
} thumb;

static _Thread_local m8_thumb_result last_result;
static _Thread_local unsigned last_boundary;
const m8_thumb_result *m8_thumb_last_result(void) { return &last_result; }
unsigned m8_thumb_last_boundary(void) { return last_boundary; }

unsigned m8_thumb_decode_boundary(uint32_t w, unsigned width,
                                  uint32_t control, uint32_t ipsr) {
    if(width==2) {
        unsigned h=w&65535,op=(h>>8)&3,rd=(h&7)|((h>>4)&8);
        if((h&0xf800)==0xe000 || (h&0xf500)==0xb100 ||
           ((h&0xf000)==0xd000 && ((h>>8)&15)<14) ||
           ((h&0xfc00)==0x4400 && (op==3 || (rd==15 && (op==0 || op==2)))) ||
           ((h&0xfe00)==0xbc00 && (h&256)))return M8_THUMB_TB_BRANCH;
        if((h&0xff00)==0xbe00 || (h&0xfe00)==0xde00)return M8_THUMB_TB_EXCEPTION;
        if((h&0xffec)==0xb660 && (h&3) && (ipsr || !(control&1)))
            return M8_THUMB_TB_CONTROL;
        if(h==0xbf10 || h==0xbf20 || h==0xbf30)return M8_THUMB_TB_HINT;
        return M8_THUMB_TB_CONTINUE;
    }
    if(width!=4)return M8_THUMB_TB_CONTINUE;
    unsigned h=w>>16,l=w&65535,rt=l>>12;
    if((w&0xfffffff0)==0xf3bf8f60)return M8_THUMB_TB_ISB;
    if((h&0xfff0)==0xf380 && (l&0xf000)==0x8000)return M8_THUMB_TB_CONTROL;
    if((w&0xffff0fff)==0xeee10a10 || (w&0xffff0fff)==0xeee80a10)
        return M8_THUMB_TB_CONTROL; /* VMSR FPSCR/FPEXC */
    if(w==0xf3af8001 || w==0xf3af8002 || w==0xf3af8003)return M8_THUMB_TB_HINT;
    if((w&0xfff0f000)==0xf7f0a000)return M8_THUMB_TB_EXCEPTION;
    /* Exclude system encodings before recognizing the branch space. */
    if((h&0xf800)==0xf000 && (l&0x8000)) {
        unsigned cond=(h>>6)&15;
        if((l&0xd000)==0x9000 || (l&0xd000)==0xd000 ||
           ((l&0xd000)==0x8000 && cond<14))return M8_THUMB_TB_BRANCH;
        return M8_THUMB_TB_CONTINUE;
    }
    if((((h&0xfff0)==0xf8d0 || (h&0xfff0)==0xf850) && rt==15) ||
       (((h&0xffc0)==0xe880 || (h&0xffc0)==0xe900) && (h&16) && (l&0x8000)) ||
       ((h&0xfff0)==0xe8d0 && (l&0xffe0)==0xf000))return M8_THUMB_TB_BRANCH;
    return M8_THUMB_TB_CONTINUE;
}
static uint32_t reg(thumb *t, unsigned r) { return r == 15 ? t->pc + 4 : t->c->r[r]; }
static int branch(thumb *t, uint32_t target, int exchange) {
    t->next = target & ~1u;
    if(exchange) {
        t->c->xpsr=(t->c->xpsr&~0x01000000u)|((target&1)<<24);
        t->a->cpsr=(t->a->cpsr&~32u)|((target&1)<<5);
        if(!(target&1)) {
            t->c->r[15]=t->next;t->reason=M8_THUMB_REASON_STATE;return 1;
        }
    }
    return 0;
}
static int read_mem(thumb *t, uint32_t addr, unsigned size, uint32_t *v) {
    uint8_t *p=m8_pointer(t->m,addr,size);
    if(p) {*v=m8_le_read(p,size);return 0;}
    /* QEMU splits an unaligned slow load into two aligned loads of the
     * original width. Preserve this bus order, including a first-read fault. */
    if(addr&(size-1)) {
        uint32_t lo,hi,base=addr&~(size-1u),bits=(addr-base)*8;
        if(read_mem(t,base,size,&lo) || read_mem(t,base+size,size,&hi))return 1;
        *v=((lo>>bits)|(hi<<(size*8-bits)))&(size==4?UINT32_MAX:(1u<<(size*8))-1);return 0;
    }
    t->address=addr;t->size=size;t->write=0;
    int result=m8_native_read(t->m,addr,size,v,&t->slow);
    if(result)t->reason=M8_THUMB_REASON_MEMORY;
    else if(size<4)*v&=(1u<<(size*8))-1;
    return result;
}
static int write_mem(thumb *t, uint32_t addr, unsigned size, uint32_t v) {
    if(size<4)v&=(1u<<(size*8))-1;
    uint8_t *p=m8_region_pointer(&t->m->dtcm,addr,size);
    if(!p)p=m8_region_pointer(&t->m->ocram,addr,size);
    if(p) {m8_le_write(p,size,v);return 0;}
    /* Slow unaligned stores issue byte writes, in ascending address order. */
    if(addr&(size-1)) {
        for(unsigned i=0;i<size;++i)if(write_mem(t,addr+i,1,(v>>(i*8))&255))return 1;
        return 0;
    }
    t->address=addr;t->size=size;t->write=1;
    int result=m8_native_write(t->m,addr,size,v,&t->slow);
    if(result)t->reason=M8_THUMB_REASON_MEMORY;
    return result;
}
static int bad_gpr(unsigned r) { return r==13 || r==15; }
static unsigned bitcount(uint32_t mask) {
    unsigned n=0; for (;mask;mask &= mask-1) ++n; return n;
}
static int transfer(thumb *t, unsigned rt, uint32_t addr, unsigned size,
                    int load, int sign, unsigned rn, int wb, uint32_t update) {
    uint32_t value;
    if (load) {
        if (read_mem(t,addr,size,&value)) return 1;
        if (sign) value=(uint32_t)m8_signed(value,size*8);
        if (wb) t->c->r[rn]=update;
        if (rt==15) return branch(t,value,1);
        t->c->r[rt]=value;
    } else {
        if (write_mem(t,addr,size,reg(t,rt))) return 1;
        if (wb) t->c->r[rn]=update;
    }
    return 0;
}
static int multiple(thumb *t, unsigned rn, uint32_t list, int load,
                    int decrement, int wb) {
    if (!list || rn==15 || (load && wb && (list & (1u<<rn)))) return 2;
    unsigned n=bitcount(list);
    uint32_t base=t->c->r[rn], addr=decrement ? base-4*n : base,target=0;
    /* Publish each completed load. A later fault must retain its prefix. */
    for (unsigned r=0;r<16;++r) if (list & (1u<<r)) {
        uint32_t value;
        if (load) {
            if (read_mem(t,addr,4,&value)) return 1;
            if (r==15) target=value;
            else t->c->r[r]=value;
        } else if (write_mem(t,addr,4,reg(t,r))) return 1;
        addr+=4;
    }
    if (wb) t->c->r[rn]=decrement ? base-4*n : base+4*n;
    if(load && (list&0x8000))return branch(t,target,1);
    return 0;
}

static int thumb16(thumb *t, uint32_t h) {
    m8_cpu *c=t->c;
    unsigned rd=h&7, rn=(h>>3)&7, rm=(h>>6)&7;
    uint32_t a,b,r,carry=(c->xpsr>>29)&1;
    int flags=!t->in_it;
    t->family="data16";
    if ((h & 0xe000)==0 && (h & 0x1800)!=0x1800) {
        unsigned op=(h>>11)&3, n=(h>>6)&31;
        if (!n && op) n=32;
        r=m8_shift(c->r[rn],op==0?2:op==1?3:1,n,&carry);
        c->r[rd]=r;
        if (flags) m8_logic_flags(c,r,carry);
        return 0;
    }
    if ((h & 0xf800)==0x1800) {
        a=c->r[rn]; b=h&0x400 ? rm : c->r[rm];
        r=h&0x200 ? (flags?m8_sub_flags(c,a,b,0):a-b) :
                       (flags?m8_add_flags(c,a,b,0):a+b);
        c->r[rd]=r; return 0;
    }
    if ((h & 0xe000)==0x2000) {
        rd=(h>>8)&7; a=c->r[rd]; b=h&255;
        switch ((h>>11)&3) {
        case 0: c->r[rd]=b; if(flags)m8_nz(c,b); break;
        case 1: (void)m8_sub_flags(c,a,b,0); break;
        case 2: c->r[rd]=flags?m8_add_flags(c,a,b,0):a+b; break;
        case 3: c->r[rd]=flags?m8_sub_flags(c,a,b,0):a-b; break;
        }
        return 0;
    }
    if ((h & 0xfc00)==0x4000) {
        a=c->r[rd]; b=c->r[rn];
        unsigned op=(h>>6)&15;
        switch(op) {
        case 0:r=a&b;break; case 1:r=a^b;break;
        case 2:case 3:case 4:case 7:
            r=m8_shift(a,op==2?7:op==3?8:op==4?6:9,b,&carry);break;
        case 5:r=flags?m8_add_flags(c,a,b,carry):a+b+carry;break;
        case 6:r=flags?m8_sub_flags(c,a,b,1-carry):a-b-(1-carry);break;
        case 8:m8_logic_flags(c,a&b,carry);return 0;
        case 9:r=flags?m8_sub_flags(c,0,b,0):0-b;break;
        case 10:(void)m8_sub_flags(c,a,b,0);return 0;
        case 11:(void)m8_add_flags(c,a,b,0);return 0;
        case 12:r=a|b;break; case 13:r=a*b;break;
        case 14:r=a&~b;break; default:r=~b;break;
        }
        c->r[rd]=r;
        if(flags && op!=5 && op!=6 && op!=9) m8_logic_flags(c,r,carry);
        return 0;
    }
    if ((h & 0xfc00)==0x4400) {
        rd=(h&7)|((h>>4)&8); rm=(h>>3)&15;
        a=reg(t,rd); b=reg(t,rm);
        switch((h>>8)&3) {
        case 0: if(rd==15)return branch(t,a+b,0); c->r[rd]=a+b;return 0;
        case 1:(void)m8_sub_flags(c,a,b,0);return 0;
        case 2:if(rd==15)return branch(t,b,0);c->r[rd]=b;return 0;
        default:
            t->family="branch16";
            if(h&7 || rm==15) return 2;
            if(h&128)c->r[14]=(t->pc+2)|1;
            return branch(t,b,1);
        }
    }
    t->family="memory16";
    if ((h & 0xf800)==0x4800)
        return transfer(t,(h>>8)&7,((t->pc+4)&~3u)+(h&255)*4,4,1,0,0,0,0);
    if ((h & 0xf000)==0x5000) {
        unsigned op=(h>>9)&7, size=op==1||op==5||op==7?2:op==2||op==3||op==6?1:4;
        return transfer(t,rd,c->r[rn]+c->r[rm],size,op>=3,op==3||op==7,0,0,0);
    }
    if ((h & 0xe000)==0x6000 || (h & 0xf000)==0x8000) {
        unsigned size=(h&0xf000)==0x8000?2:h&0x1000?1:4;
        return transfer(t,rd,c->r[rn]+((h>>6)&31)*size,size,(h>>11)&1,0,0,0,0);
    }
    if ((h & 0xf000)==0x9000)
        return transfer(t,(h>>8)&7,c->r[13]+(h&255)*4,4,(h>>11)&1,0,0,0,0);
    if ((h & 0xf000)==0xa000) {
        c->r[(h>>8)&7]=(h&0x800?c->r[13]:(t->pc+4)&~3u)+(h&255)*4;return 0;
    }
    if ((h & 0xff00)==0xb000) {
        c->r[13]+=h&128 ? 0u-(h&127)*4 : (h&127)*4;return 0;
    }
    if ((h & 0xf600)==0xb400)
        return multiple(t,13,(h&255)|((h&256)?(h&0x800?0x8000:0x4000):0),(h>>11)&1,!(h&0x800),1);
    if ((h & 0xf000)==0xc000) {
        rn=(h>>8)&7;
        return multiple(t,rn,h&255,(h>>11)&1,0,!(h&0x800) || !(h&(1u<<rn)));
    }
    t->family="branch16";
    if ((h & 0xf500)==0xb100) {
        if(t->in_it)return 2;
        if((c->r[rd]!=0)==!!(h&0x800))t->next=t->pc+4+((h>>3)&0x40)+((h>>2)&0x3e);
        return 0;
    }
    if ((h & 0xf000)==0xd000) {
        unsigned cond=(h>>8)&15;
        if(cond>=14) {
            t->reason=cond==15?M8_THUMB_REASON_SVC:M8_THUMB_REASON_UNDEFINED;
            t->family=cond==15?"svc16":"undefined16";return 1;
        }
        if(m8_condition(c->xpsr,cond+1))t->next=t->pc+4+(uint32_t)(m8_signed(h&255,8)*2);
        return 0;
    }
    if ((h & 0xf800)==0xe000) {
        t->next=t->pc+4+(uint32_t)(m8_signed(h&2047,11)*2);return 0;
    }
    t->family="extend16";
    if ((h & 0xff00)==0xb200) {
        b=c->r[rn];
        switch((h>>6)&3) {
        case 0:r=(uint32_t)m8_signed(b,16);break;
        case 1:r=(uint32_t)m8_signed(b,8);break;
        case 2:r=b&65535;break;default:r=b&255;break;
        }
        c->r[rd]=r;return 0;
    }
    if ((h & 0xff00)==0xba00) {
        b=c->r[rn];
        switch((h>>6)&3) {
        case 0:r=(b>>24)|((b>>8)&0xff00)|((b<<8)&0xff0000)|(b<<24);break;
        case 1:r=((b>>8)&0x00ff00ff)|((b<<8)&0xff00ff00);break;
        case 3:r=(uint32_t)m8_signed(((b&255)<<8)|((b>>8)&255),16);break;
        default:return 2;
        }
        c->r[rd]=r;return 0;
    }
    t->family="system16";t->special=1;
    if((h&0xff00)==0xbe00) {
        t->family="breakpoint16";t->reason=M8_THUMB_REASON_BREAKPOINT;return 1;
    }
    if ((h & 0xff00)==0xbf00) {
        if(h&15) {
            if(t->in_it || ((h>>4)&15)>=14)return 2;
            t->a->itstate=h&255;m8_native_publish_it(c,t->a);return 0;
        }
        if(h==0xbf30) {t->family="wfi16";t->reason=M8_THUMB_REASON_WAIT;return 0;}
        if(h==0xbf20 || h==0xbf40) {
            t->family=h==0xbf20?"wfe16":"sev16";t->reason=M8_THUMB_REASON_EVENT;return 1;
        }
        if(h==0xbf10) {t->family="yield16";t->reason=M8_THUMB_REASON_YIELD;return 1;}
        return h==0xbf00 ? 0 : 2;
    }
    if((h&0xffec)==0xb660 && (h&3)) {
        if(t->a->ipsr || !(t->a->control&1)) {
            if(h&2)t->a->primask=(h>>4)&1;
            if(h&1)t->a->faultmask=(h>>4)&1;
        }
        return 0;
    }
    return 2;
}

static uint32_t expand_imm(unsigned imm, uint32_t *carry) {
    uint32_t b=imm&255;
    if (!(imm&0xc00)) {
        switch((imm>>8)&3) {
        case 0:return b;case 1:return b|(b<<16);
        case 2:return (b<<8)|(b<<24);default:return b*0x01010101u;
        }
    }
    b=m8_ror(0x80u|(imm&127),(imm>>7)&31);*carry=b>>31;return b;
}
static unsigned shift_type(unsigned op) {
    const unsigned types[]={2,3,1,4};return types[op&3];
}
static int data32(thumb *t, unsigned op, unsigned rd, unsigned rn,
                   uint32_t b, uint32_t carry, int flags) {
    m8_cpu *c=t->c;
    int test=rd==15;
    if(test && (!flags || (op!=0 && op!=4 && op!=8 && op!=13)))return 2;
    if(rn==15 && op!=2 && op!=3)return 2;
    uint32_t a=reg(t,rn), r;
    switch(op) {
    case 0:r=a&b;break;case 1:r=a&~b;break;
    case 2:r=rn==15?b:a|b;break;case 3:r=rn==15?~b:a|~b;break;
    case 4:r=a^b;break;
    case 8:r=flags?m8_add_flags(c,a,b,0):a+b;break;
    case 10:r=flags?m8_add_flags(c,a,b,(c->xpsr>>29)&1):a+b+((c->xpsr>>29)&1);break;
    case 11:r=flags?m8_sub_flags(c,a,b,1-((c->xpsr>>29)&1)):a-b-(1-((c->xpsr>>29)&1));break;
    case 13:r=flags?m8_sub_flags(c,a,b,0):a-b;break;
    case 14:r=flags?m8_sub_flags(c,b,a,0):b-a;break;
    default:return 2;
    }
    if(flags && op<8)m8_logic_flags(c,r,carry);
    if(!test)c->r[rd]=r;
    return 0;
}

static unsigned fpreg(uint32_t w, int dp, unsigned which) {
    unsigned d=((w>>12)&15),n=((w>>16)&15),m=w&15;
    if(dp) { d|=(w>>18)&16;n|=(w>>3)&16;m|=(w>>1)&16; }
    else { d=d*2+((w>>22)&1);n=n*2+((w>>7)&1);m=m*2+((w>>5)&1); }
    return which==0?d:which==1?n:m;
}
static uint64_t fpget(m8_cpu *c, unsigned r, int dp) {
    return dp?m8_fp_get64(c,r):c->s[r];
}
static void fpput(m8_cpu *c, unsigned r, int dp, uint64_t value) {
    if(dp)m8_fp_set64(c,r,value);else c->s[r]=(uint32_t)value;
}
static uint64_t fpop(m8_cpu *c, int dp, int op, uint64_t a, uint64_t b, uint64_t d) {
    return dp?m8_fp_op64(c,op,a,b,d):m8_fp_eval32(c,op,(uint32_t)a,(uint32_t)b,(uint32_t)d);
}
/* Encoding rules and numeric helpers are shared with the maintained AOT FP
 * emitter. In particular, negative fused encodings do not use mnemonic names. */
static int floating(thumb *t) {
    uint32_t w=t->w; m8_cpu *c=t->c;
    int dp=((w>>8)&15)==11;
    unsigned d=fpreg(w,dp,0),n=fpreg(w,dp,1),m=fpreg(w,dp,2);
    uint64_t sign=dp?UINT64_C(0x8000000000000000):UINT64_C(0x80000000);
    if((w&0xffe00fff)==0xeee00a10) {
        unsigned rt=(w>>12)&15;
        if(((w>>16)&15)!=1 || (!(w&(1u<<20)) && rt==15))return 2;
        if(w&(1u<<20)) {
            m8_fp_sync(c);
            if(rt==15)c->xpsr=(c->xpsr&0x0fffffff)|(c->fpscr&0xf0000000);
            else c->r[rt]=c->fpscr;
        } else {c->fpscr=c->r[rt]&0xf7c0009f;m8_fp_clear();m8_fp_configure(c);}
        return 0;
    }
    if((w&0xffe00f7f)==0xee000a10) {
        unsigned s=((w>>16)&15)*2+((w>>7)&1),rt=(w>>12)&15;
        if(rt==15)return 2;
        if(w&(1u<<20))c->r[rt]=c->s[s];else c->s[s]=c->r[rt];return 0;
    }
    if((w&0xffc00f7f)==0xee000b10) {
        unsigned dn=((w>>16)&15)|((w>>3)&16),rt=(w>>12)&15;
        if(dn>=16 || rt==15)return 2;
        unsigned idx=dn*2+((w>>21)&1);
        if(w&(1u<<20))c->r[rt]=c->s[idx];else c->s[idx]=c->r[rt];return 0;
    }
    if((w&0xffe00fd0)==0xec400a10 || (w&0xffe00fd0)==0xec400b10) {
        unsigned rt=(w>>12)&15,rt2=(w>>16)&15,idx=dp?m*2:m;
        int load=!!(w&(1u<<20));
        if(idx+1>=32 || rt==15 || rt2==15 || (load && rt==rt2))return 2;
        if(load) {c->r[rt]=c->s[idx];c->r[rt2]=c->s[idx+1];}
        else {c->s[idx]=c->r[rt];c->s[idx+1]=c->r[rt2];}return 0;
    }
    if((w>>24)==0xec || (w>>24)==0xed) {
        int p=(w>>24)&1,u=(w>>23)&1,wb=(w>>21)&1,load=(w>>20)&1;
        unsigned rn=(w>>16)&15,imm=w&255,first=dp?d*2:d,count;
        uint32_t base=rn==15?(t->pc+4)&~3u:c->r[rn],addr;
        if(p && !wb) {count=dp?2:1;addr=u?base+imm*4:base-imm*4;}
        else if((!p && u) || (p && !u && wb)) {
            if(rn==15 || !imm || (dp && (imm&1)))return 2;
            count=imm;addr=u?base:base-imm*4;
        } else return 2;
        if(first+count>32)return 2;
        for(unsigned i=0;i<count;++i) {
            uint32_t value;
            if(load) {if(read_mem(t,addr+i*4,4,&value))return 1;c->s[first+i]=value;}
            else if(write_mem(t,addr+i*4,4,c->s[first+i]))return 1;
        }
        if(wb)c->r[rn]=u?base+imm*4:base-imm*4;
        return 0;
    }
    if(w&16)return 2;
    unsigned major=(w>>20)&11,sub=(w>>16)&15;
    int bit6=!!(w&64);
    if((w>>24)==0xfe) {
        if(!(w&(1u<<23)) && !bit6) {
            if(dp && (d>=16 || n>=16 || m>=16))return 2;
            uint32_t p=c->xpsr;
            int condition;
            switch((w>>20)&3) {
            case 0:condition=!!(p&0x40000000);break;
            case 1:condition=!!(p&0x10000000);break;
            case 2:condition=((p>>31)&1)==((p>>28)&1);break;
            default:condition=!(p&0x40000000) && (((p>>31)&1)==((p>>28)&1));break;
            }
            fpput(c,d,dp,fpget(c,condition?n:m,dp));return 0;
        }
        if(major==8) {
            if(dp && (d>=16 || n>=16 || m>=16))return 2;
            fpput(c,d,dp,fpop(c,dp,bit6?M8_FP_MINNM:M8_FP_MAXNM,fpget(c,n,dp),fpget(c,m,dp),0));return 0;
        }
        if(major==11 && bit6) {
            const unsigned modes[]={4,0,1,2};unsigned mode=modes[sub&3];
            if(sub>=8 && sub<=11 && !(w&128)) {
                if(dp && (d>=16 || m>=16))return 2;
                fpput(c,d,dp,m8_fp_rint(c,fpget(c,m,dp),dp,mode,0));return 0;
            }
            if(sub>=12) {
                if(dp && m>=16)return 2;
                c->s[fpreg(w,0,0)]=m8_fp_to_int(c,fpget(c,m,dp),dp,!(w&128),mode,32,0);return 0;
            }
        }
        return 2;
    }
    if(major==0 || major==1 || major==2 || major==3 || major==8 || major==9 || major==10) {
        if(dp && (d>=16 || n>=16 || m>=16))return 2;
        if(major==8 && bit6)return 2;
        uint64_t aa=fpget(c,n,dp),bb=fpget(c,m,dp),dd=fpget(c,d,dp),r;
        if(major<=1) {
            r=fpop(c,dp,M8_FP_MUL,aa,bb,0);
            if(bit6)r^=sign;
            r=fpop(c,dp,M8_FP_ADD,major==1?dd^sign:dd,r,0);
        } else if(major==2) {r=fpop(c,dp,M8_FP_MUL,aa,bb,0);if(bit6)r^=sign;}
        else if(major==3)r=fpop(c,dp,bit6?M8_FP_SUB:M8_FP_ADD,aa,bb,0);
        else if(major==8)r=fpop(c,dp,M8_FP_DIV,aa,bb,0);
        else r=fpop(c,dp,M8_FP_FMA,bit6?aa^sign:aa,bb,major==9?dd^sign:dd);
        fpput(c,d,dp,r);return 0;
    }
    if(major!=11)return 2;
    if(!(w&0xe0)) {
        if(dp && d>=16)return 2;
        unsigned imm=((w>>12)&0xf0)|(w&15);
        uint64_t r=dp?((uint64_t)((imm&128?0x8000:0)|(imm&64?0x3fc0:0x4000)|(imm&63))<<48):
             (uint32_t)(((imm&128?0x8000:0)|(imm&64?0x3e00:0x4000)|((imm&63)<<3))<<16);
        fpput(c,d,dp,r);return 0;
    }
    if(!bit6)return 2;
    if(dp) {
        int d_fp=sub!=12 && sub!=13 && !(sub==7 && (w&128));
        int m_fp=sub==0 || sub==1 || sub==4 || sub==6 || sub==7 || sub==12 || sub==13;
        if((d_fp && d>=16) || (m_fp && m>=16))return 2;
    }
    if(sub==0 || sub==1) {
        uint64_t b=fpget(c,m,dp),r;
        if(sub==0)r=w&128?b&~sign:b;
        else r=w&128?fpop(c,dp,M8_FP_SQRT,b,0,0):b^sign;
        fpput(c,d,dp,r);return 0;
    }
    if(sub==4 || sub==5) {
        uint64_t aa=fpget(c,d,dp),bb=sub==5?0:fpget(c,m,dp);
        if(dp)m8_fp_cmp64(c,aa,bb,!!(w&128));else m8_fp_cmp32(c,(uint32_t)aa,(uint32_t)bb,!!(w&128));
        return 0;
    }
    if(sub==6 || (sub==7 && !(w&128))) {
        unsigned mode=sub==6 && (w&128)?3:(c->fpscr>>22)&3;
        fpput(c,d,dp,m8_fp_rint(c,fpget(c,m,dp),dp,mode,sub==7));return 0;
    }
    if(sub==7) {
        unsigned dest=fpreg(w,!dp,0);
        if(!dp && dest>=16)return 2;
        fpput(c,dest,!dp,m8_fp_convert(c,fpget(c,m,dp),dp));return 0;
    }
    if(sub==8) {
        fpput(c,d,dp,m8_fp_from_int(c,c->s[fpreg(w,0,2)],dp,!(w&128),32,0,0));return 0;
    }
    if(sub==12 || sub==13) {
        unsigned mode=w&128?3:(c->fpscr>>22)&3;
        c->s[fpreg(w,0,0)]=m8_fp_to_int(c,fpget(c,m,dp),dp,sub==12,mode,32,0);return 0;
    }
    if(sub==10 || sub==11 || sub==14 || sub==15) {
        unsigned bits=w&128?32:16,imm=(w&15)*2+((w>>5)&1);
        if(imm>bits)return 2;
        uint64_t r;
        if(sub&4) {
            r=m8_fp_to_int(c,fpget(c,d,dp),dp,sub&1,3,bits,bits-imm);
            if(dp && !(sub&1))r=(uint64_t)m8_s32((uint32_t)r);
        } else {
            /* Preserve the pinned reference's directed fixed-point rounding.
             * The AOT emitter intentionally uses the architectural RN rule. */
            r=m8_fp_from_int(c,(uint32_t)fpget(c,d,dp),dp,sub&1,bits,bits-imm,0);
        }
        fpput(c,d,dp,r);return 0;
    }
    return 2;
}

static int exclusive(thumb *t, unsigned rt, unsigned rd, uint32_t addr,
                      unsigned size, int store) {
    m8_native_arch *a=t->a;m8_cpu *c=t->c;
    if(store && (!a->exclusive_valid || a->exclusive_address!=addr)) {
        c->r[rd]=1;a->exclusive_valid=0;return 0;
    }
    if(addr&(size-1)) {t->reason=M8_THUMB_REASON_MEMORY;t->address=addr;t->size=size;t->write=store;return 1;}
    uint32_t value;
    if(read_mem(t,addr,size,&value))return 1;
    if(store) {
        uint32_t fail=value!=a->exclusive_value;
        if(!fail && write_mem(t,addr,size,c->r[rt]))return 1;
        c->r[rd]=fail;a->exclusive_valid=0;
    } else {
        c->r[rt]=value;a->exclusive_address=addr;a->exclusive_value=value;a->exclusive_valid=1;
    }
    return 0;
}

static int thumb32(thumb *t) {
    m8_cpu *c=t->c;m8_native_arch *a=t->a;
    uint32_t w=t->w,h=w>>16,l=w&65535;
    unsigned rn=h&15,rd=(l>>8)&15,rm=l&15,rt=l>>12;
    uint32_t carry=(c->xpsr>>29)&1,b,r;
    if((((w>>24)&0xfe)==0xec || (w>>24)==0xee || (w>>24)==0xfe) &&
       (((w>>8)&15)==10 || ((w>>8)&15)==11)) {
        t->family="floating32";
        if(!(a->control&4) && !t->fp_prepared) {t->family="floating-context";t->reason=M8_THUMB_REASON_FP_CONTEXT;return 2;}
        m8_fp_env env=m8_fp_enter(c);
        int result=floating(t);m8_fp_leave(env);return result;
    }
    t->family="system32";
    if((w&0xfffffff0)==0xf3bf8f20) {a->exclusive_valid=0;t->special=1;return 0;}
    if((w&0xfffffff0)==0xf3bf8f40 || (w&0xfffffff0)==0xf3bf8f50 || (w&0xfffffff0)==0xf3bf8f60) {
        __asm__ volatile("" ::: "memory");t->special=1;return 0;
    }
    if(h==0xf3ef && (l&0xf000)==0x8000) {
        if(bad_gpr(rd))return 2;
        if(m8_native_mrs(c,a,l&255,&r))return 2;
        c->r[rd]=r;t->special=1;return 0;
    }
    if((h&0xfff0)==0xf380 && (l&0xf000)==0x8000) {
        if((l&255)==20 && !bad_gpr(rn)) {
            m8_native_save_sp(c,a);
            if(a->ipsr || !(a->control&1)) {
                uint32_t mask=a->ipsr?5:7;
                a->control=(a->control&~mask)|(c->r[rn]&mask);
                c->r[13]=m8_native_using_psp(a)?a->psp:a->msp;
            }
            t->special=1;return 0;
        }
        if(bad_gpr(rn) || m8_native_msr(c,a,l&255,(l>>8)&15,c->r[rn]))return 2;
        t->special=1;return 0;
    }
    if(w==0xf3af8000) {t->special=1;return 0;}
    /* DBG is a hint; the reference has no attached debug-monitor service. */
    if((w&0xfffffff0)==0xf3af80f0) {t->special=1;return 0;}
    if(w==0xf3af8003) {t->special=1;t->family="wfi32";t->reason=M8_THUMB_REASON_WAIT;return 0;}
    if(w==0xf3af8002 || w==0xf3af8004) {
        t->family=w==0xf3af8002?"wfe32":"sev32";t->reason=M8_THUMB_REASON_EVENT;return 1;
    }
    if(w==0xf3af8001) {t->family="yield32";t->reason=M8_THUMB_REASON_YIELD;return 1;}
    if((w&0xfff0f000)==0xf7f0a000) {
        t->family="undefined32";t->reason=M8_THUMB_REASON_UNDEFINED;return 1;
    }
    t->family="branch32";
    if((h&0xf800)==0xf000 && (l&0x8000)) {
        unsigned s=(h>>10)&1,j1=(l>>13)&1,j2=(l>>11)&1;
        if((l&0xd000)==0x8000) {
            unsigned cond=(h>>6)&15;
            if(cond>=14 || t->in_it)return 2;
            r=(s<<20)|(j2<<19)|(j1<<18)|((h&63)<<12)|((l&2047)<<1);
            if(m8_condition(c->xpsr,cond+1))t->next=t->pc+4+(uint32_t)m8_signed(r,21);
            return 0;
        }
        if((l&0xd000)!=0x9000 && (l&0xd000)!=0xd000)return 2;
        r=(s<<24)|((!(j1^s))<<23)|((!(j2^s))<<22)|((h&1023)<<12)|((l&2047)<<1);
        if(l&0x4000)c->r[14]=(t->pc+4)|1;
        t->next=t->pc+4+(uint32_t)m8_signed(r,25);return 0;
    }
    t->family="data32";
    if((h&0xfe00)==0xea00 && !(l&0x8000)) {
        unsigned op=(h>>5)&15,type=(l>>4)&3,n=((l>>12)&7)*4+((l>>6)&3);
        if(rm==15)return 2;
        if(!n && (type==1 || type==2))n=32;
        if(type==3 && !n)type=4; /* RRX uses helper identifier 5. */
        b=m8_shift(reg(t,rm),type==4?5:shift_type(type),n,&carry);
        if(op==6) { /* PKHBT/PKHTB */
            if(bad_gpr(rd) || bad_gpr(rn) || (h&16) || (l&16))return 2;
            c->r[rd]=type==2?(c->r[rn]&0xffff0000u)|(b&65535):
                                      (c->r[rn]&65535)|(b&0xffff0000u);return 0;
        }
        return data32(t,op,rd,rn,b,carry,!!(h&16));
    }
    if((h&0xfa00)==0xf000 && !(l&0x8000)) {
        unsigned imm=((h>>10)&1)*2048+((l>>12)&7)*256+(l&255);
        if(!(imm&0xc00) && (imm&0x300) && !(imm&255))return 2;
        b=expand_imm(imm,&carry);
        return data32(t,(h>>5)&15,rd,rn,b,carry,!!(h&16));
    }
    if(((h&0xfbf0)==0xf200 || (h&0xfbf0)==0xf2a0) && !(l&0x8000)) {
        if(rd==15)return 2;
        b=((h>>10)&1)*2048+((l>>12)&7)*256+(l&255);
        r=rn==15?(t->pc+4)&~3u:c->r[rn];
        c->r[rd]=(h&128)?r-b:r+b;return 0;
    }
    if(((h&0xfbf0)==0xf240 || (h&0xfbf0)==0xf2c0) && !(l&0x8000)) {
        if(bad_gpr(rd))return 2;
        b=(h&15)*4096+((h>>10)&1)*2048+((l>>12)&7)*256+(l&255);
        c->r[rd]=(h&128)?(c->r[rd]&65535)|(b<<16):b;return 0;
    }
    t->family="bitfield32";
    if(((h&0xfbf0)==0xf340 || (h&0xfbf0)==0xf3c0 || (h&0xfbf0)==0xf360) && !(l&0x8020)) {
        if(bad_gpr(rd))return 2;
        unsigned pos=((l>>12)&7)*4+((l>>6)&3),n=(l&31)+1;
        if((h&0xfbf0)==0xf360) {
            if(n<=pos)return 2;
            n-=pos;
            uint32_t mask=n==32?UINT32_MAX:((1u<<n)-1);
            b=rn==15?0:c->r[rn];
            c->r[rd]=(c->r[rd]&~(mask<<pos))|((b&mask)<<pos);return 0;
        }
        if(bad_gpr(rn) || pos+n>32)return 2;
        b=c->r[rn]>>pos;
        c->r[rd]=(h&128)?(n==32?b:b&((1u<<n)-1)):(uint32_t)m8_signed(b,n);return 0;
    }
    if(((h&0xfbd0)==0xf300 || (h&0xfbd0)==0xf380) && !(l&0x8020)) {
        if(bad_gpr(rd) || bad_gpr(rn))return 2;
        int uns=!!(h&128);unsigned n=(l&31)+(uns?0:1);
        unsigned shift=((l>>12)&7)*4+((l>>6)&3);
        if((h&32) && !shift) { /* SSAT16 / USAT16 */
            if(l&0x70c0 || n>(uns?15u:16u))return 2;
            uint32_t lo=(uint32_t)m8_signed(c->r[rn],16),hi=(uint32_t)m8_signed(c->r[rn]>>16,16);
            lo=uns?m8_sat_unsigned(c,m8_s32(lo),n):m8_sat_signed(c,m8_s32(lo),n);
            hi=uns?m8_sat_unsigned(c,m8_s32(hi),n):m8_sat_signed(c,m8_s32(hi),n);
            c->r[rd]=(lo&65535)|(hi<<16);return 0;
        }
        b=h&32?m8_asr(c->r[rn],shift):c->r[rn]<<shift;
        c->r[rd]=uns?m8_sat_unsigned(c,m8_s32(b),n):m8_sat_signed(c,m8_s32(b),n);return 0;
    }
    t->family="memory32";
    /* Single transfers: positive imm12, register, and indexed imm8 forms. */
    if((h&0xfe00)==0xf800) {
        unsigned op=(h>>4)&31,size;
        int load=op&1,sign=op&16;
        unsigned kind=(op>>1)&3;
        if(kind>2 || (sign && !load))return 2;
        size=kind==0?1:kind==1?2:4;
        if(sign && size==4)return 2;
        uint32_t base=rn==15?(t->pc+4)&~3u:c->r[rn],addr,update=0;
        int wb=0;
        if(rt==15 && size!=4) {
            /* PLD/PLI hints perform no architectural data access. */
            if(load) return 0;
            return 2;
        }
        if(rn==15) {
            if(!load)return 2;
            addr=h&128?base+(l&4095):base-(l&4095);
        } else if(h&128) addr=base+(l&4095);
        else if(l&0x800) {
            unsigned p=(l>>10)&1,u=(l>>9)&1;
            wb=!!(l&256);
            if(!p && !wb)return 2;
            if(wb && load && rn==rt)return 2;
            /* P=1,W=0,U=1 denotes an unprivileged transfer; same bus. */
            update=u?base+(l&255):base-(l&255);addr=p?update:base;
        } else {
            if(l&0xfc0 || bad_gpr(rm))return 2;
            addr=base+(c->r[rm]<<((l>>4)&3));
        }
        return transfer(t,rt,addr,size,load,sign,rn,wb,update);
    }
    if((h&0xffc0)==0xe880 || (h&0xffc0)==0xe900) {
        if(l&0x2000 || (!(h&16) && (l&0x8000)))return 2;
        return multiple(t,rn,l,!!(h&16),!!(h&256),!!(h&32));
    }
    if((h&0xfe40)==0xe840 && (h&0x120)) {
        int load=!!(h&16),p=!!(h&256),u=!!(h&128),wb=!!(h&32);
        unsigned rt2=(l>>8)&15;
        if(bad_gpr(rt) || bad_gpr(rt2) || (load && rt==rt2) || (rn==15 && (!load||wb)) ||
           (load && wb && (rn==rt || rn==rt2)))return 2;
        uint32_t base=rn==15?(t->pc+4)&~3u:c->r[rn],off=(l&255)*4;
        uint32_t update=u?base+off:base-off,addr=p?update:base,v1,v2;
        if(load) {
            if(read_mem(t,addr,4,&v1))return 1;
            c->r[rt]=v1;
            if(read_mem(t,addr+4,4,&v2))return 1;
            c->r[rt2]=v2;
        } else {
            v1=c->r[rt];v2=c->r[rt2];
            if(write_mem(t,addr,4,v1) || write_mem(t,addr+4,4,v2))return 1;
        }
        if(wb)c->r[rn]=update;
        return 0;
    }
    t->family="exclusive32";t->special=1;
    if((h&0xfff0)==0xe850 && (l&0xf00)==0xf00) {
        if(rn==15 || bad_gpr(rt))return 2;
        return exclusive(t,rt,0,c->r[rn]+(l&255)*4,4,0);
    }
    if((h&0xfff0)==0xe840) {
        if(rn==15 || bad_gpr(rt) || bad_gpr(rd) || rd==rn || rd==rt)return 2;
        return exclusive(t,rt,rd,c->r[rn]+(l&255)*4,4,1);
    }
    if((h&0xfff0)==0xe8d0 && ((l&0xfff)==0xf4f || (l&0xfff)==0xf5f)) {
        if(rn==15 || bad_gpr(rt))return 2;
        return exclusive(t,rt,0,c->r[rn],l&16?2:1,0);
    }
    if((h&0xfff0)==0xe8c0 && ((l&0xff0)==0xf40 || (l&0xff0)==0xf50)) {
        if(rn==15 || bad_gpr(rt) || bad_gpr(rm) || rm==rn || rm==rt)return 2;
        return exclusive(t,rt,rm,c->r[rn],l&16?2:1,1);
    }
    t->special=0;t->family="table-branch32";
    if((h&0xfff0)==0xe8d0 && (l&0xffe0)==0xf000) {
        if(bad_gpr(rm))return 2;
        unsigned size=l&16?2:1;
        if(read_mem(t,reg(t,rn)+c->r[rm]*size,size,&r))return 1;
        t->next=t->pc+4+r*2;return 0;
    }
    t->family="register32";
    if((h&0xff80)==0xfa00 && (l&0xf0f0)==0xf000) {
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm))return 2;
        r=m8_shift(c->r[rn],shift_type((h>>5)&3)+5,c->r[rm],&carry);
        c->r[rd]=r;if(h&16)m8_logic_flags(c,r,carry);return 0;
    }
    if((h&0xff80)==0xfa00 && (l&0xf0c0)==0xf080) {
        unsigned op=(h>>4)&7;
        if(bad_gpr(rd) || bad_gpr(rm) || op>5)return 2;
        b=m8_ror(c->r[rm],((l>>4)&3)*8);uint32_t base=rn==15?0:c->r[rn];
        if(op==2 || op==3) {
            uint32_t low=op&1?b&255:(uint32_t)m8_signed(b,8);
            uint32_t high=op&1?(b>>16)&255:(uint32_t)m8_signed(b>>16,8);
            r=((base+low)&65535)|(((base>>16)+high)<<16);
        } else { b=op&1?b&(op<2?65535u:255u):(uint32_t)m8_signed(b,op<2?16:8);r=base+b; }
        c->r[rd]=r;return 0;
    }
    if((h&0xfff0)==0xfa90 && (l&0xf0c0)==0xf080) {
        if(bad_gpr(rd) || bad_gpr(rm) || rn!=rm)return 2;
        b=c->r[rm];
        switch((l>>4)&3) {
        case 0:r=(b>>24)|((b>>8)&0xff00)|((b<<8)&0xff0000)|(b<<24);break;
        case 1:r=((b>>8)&0x00ff00ff)|((b<<8)&0xff00ff00);break;
        case 2:r=m8_rbit(b);break;
        default:r=(uint32_t)m8_signed(((b&255)<<8)|((b>>8)&255),16);break;
        }
        c->r[rd]=r;return 0;
    }
    if((h&0xfff0)==0xfab0 && (l&0xf0f0)==0xf080) {
        if(bad_gpr(rd) || bad_gpr(rm) || rn!=rm)return 2;
        c->r[rd]=m8_clz(c->r[rm]);return 0;
    }
    t->family="packed32";
    if((h&0xff80)==0xfa80 && (l&0xf080)==0xf000) {
        unsigned op=(h>>4)&7,mode=(l>>4)&7;
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm) || op==3 || op==7 || (mode&3)==3)return 2;
        unsigned kind=op==2?2:op==6?3:op&4?1:0;
        c->r[rd]=m8_parallel(c,c->r[rn],c->r[rm],op==0 || op==4?8:16,kind,
                             (mode&3)*2+!!(mode&4));return 0;
    }
    if((h&0xfff0)==0xfa80 && (l&0xf0c0)==0xf080) {
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm))return 2;
        int64_t x=m8_s32(c->r[rm]),y=m8_s32(c->r[rn]);
        if(l&16)y=m8_s32(m8_sat_signed(c,y*2,32));
        c->r[rd]=m8_sat_signed(c,l&32?x-y:x+y,32);return 0;
    }
    if((h&0xfff0)==0xfaa0 && (l&0xf0f0)==0xf080) {
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm))return 2;
        c->r[rd]=m8_sel(c->xpsr,c->r[rn],c->r[rm]);return 0;
    }
    t->family="multiply32";
    if((h&0xfff0)==0xfb00 && (l&0xe0)==0) {
        unsigned ra=(l>>12)&15;
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm) || ra==13)return 2;
        b=c->r[rn]*c->r[rm];r=ra==15?0:c->r[ra];
        if((l&16) && ra==15)return 2;
        c->r[rd]=l&16?r-b:r+b;return 0;
    }
    if(((h&0xfff0)==0xfb90 || (h&0xfff0)==0xfbb0) && (l&0xf0f0)==0xf0f0) {
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm))return 2;
        b=c->r[rm];
        if(!b)r=0;
        else if(h&32)r=c->r[rn]/b;
        else r=(uint32_t)(m8_s32(c->r[rn])/m8_s32(b));
        c->r[rd]=r;return 0;
    }
    if((h&0xff80)==0xfb80 && !(l&0xf0)) {
        unsigned op=(h>>4)&7,lo=rt,hi=rd;
        if((op!=0 && op!=2 && op!=4 && op!=6) || bad_gpr(lo) || bad_gpr(hi) ||
           lo==hi || bad_gpr(rn) || bad_gpr(rm))return 2;
        uint64_t v=op&2?(uint64_t)c->r[rn]*c->r[rm]:
                           (uint64_t)(m8_s32(c->r[rn])*m8_s32(c->r[rm]));
        if(op&4)v+=(uint64_t)c->r[lo]|((uint64_t)c->r[hi]<<32);
        c->r[lo]=(uint32_t)v;c->r[hi]=(uint32_t)(v>>32);return 0;
    }
    t->family="dsp-multiply32";
    if((h&0xff80)==0xfb00) {
        unsigned op=(h>>4)&7,ra=rt,sub=(l>>4)&15;
        if(bad_gpr(rd) || bad_gpr(rn) || bad_gpr(rm) || ra==13)return 2;
        uint32_t x=c->r[rn],y=c->r[rm];int64_t product;
        if(op==1 && sub<=3) {
            product=m8_signed(x>>(sub&2?16:0),16)*m8_signed(y>>(sub&1?16:0),16);
            c->r[rd]=ra==15?(uint32_t)product:m8_qresult(c,product+m8_s32(c->r[ra]));return 0;
        }
        if(op==3 && sub<=1) {
            product=m8_s32(x)*m8_signed(y>>(sub?16:0),16);
            uint32_t high=(uint32_t)((uint64_t)product>>16);
            c->r[rd]=ra==15?high:m8_qresult(c,m8_s32(high)+m8_s32(c->r[ra]));return 0;
        }
        if((op==2 || op==4) && sub<=1) {
            if(sub)y=m8_ror(y,16);
            int64_t low=m8_signed(x,16)*m8_signed(y,16),high=m8_signed(x>>16,16)*m8_signed(y>>16,16);
            product=op==4?low-high:low+high;
            if(op==2)(void)m8_qresult(c,product);
            if(ra!=15)product+=m8_s32(c->r[ra]);
            c->r[rd]=(ra!=15 || op==2)?m8_qresult(c,product):(uint32_t)product;return 0;
        }
        if((op==5 || op==6) && sub<=1 && (op!=6 || ra!=15)) {
            uint64_t p=(uint64_t)(m8_s32(x)*m8_s32(y));
            if(ra!=15) {uint64_t accum=(uint64_t)c->r[ra]<<32;p=op==6?accum-p:accum+p;}
            if(sub)p+=UINT64_C(0x80000000);
            c->r[rd]=(uint32_t)(p>>32);return 0;
        }
        if(op==7 && !sub) {
            c->r[rd]=m8_usad8(x,y)+(ra==15?0:c->r[ra]);return 0;
        }
        return 2;
    }
    if((h&0xffe0)==0xfbc0) {
        unsigned op=(h>>4)&1,sub=(l>>4)&15,lo=rt,hi=rd;
        if(bad_gpr(lo) || bad_gpr(hi) || lo==hi || bad_gpr(rn) || bad_gpr(rm))return 2;
        uint32_t x=c->r[rn],y=c->r[rm];int64_t product;
        if(!op && sub>=8 && sub<=11)
            product=m8_signed(x>>(sub&2?16:0),16)*m8_signed(y>>(sub&1?16:0),16);
        else if(sub==12 || sub==13) {
            if(sub&1)y=m8_ror(y,16);
            int64_t low=m8_signed(x,16)*m8_signed(y,16),high=m8_signed(x>>16,16)*m8_signed(y>>16,16);
            product=op?low-high:low+high;
        } else return 2;
        uint64_t v=(uint64_t)product+((uint64_t)c->r[lo]|((uint64_t)c->r[hi]<<32));
        c->r[lo]=(uint32_t)v;c->r[hi]=(uint32_t)(v>>32);return 0;
    }
    if((h&0xfff0)==0xfbe0 && (l&0xf0)==0x60) {
        unsigned lo=rt,hi=rd;
        if(bad_gpr(lo) || bad_gpr(hi) || lo==hi || bad_gpr(rn) || bad_gpr(rm))return 2;
        uint64_t v=(uint64_t)c->r[rn]*c->r[rm]+c->r[lo]+c->r[hi];
        c->r[lo]=(uint32_t)v;c->r[hi]=(uint32_t)(v>>32);return 0;
    }
    t->family="unknown32";return 2;
}

static int mapped_fetch(void *ctx, uint32_t pc, unsigned size, uint8_t *bytes) {
    uint8_t *p=m8_pointer((m8_mem*)ctx,pc,size);
    if(!p)return 1;
    memcpy(bytes,p,size);return 0;
}

static int interpret(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch,
                     m8_thumb_fetch_fn fetch, void *ctx, int fp_prepared) {
    thumb t={.c=cpu,.m=mem,.a=arch,.pc=cpu->r[15],.in_it=arch->itstate!=0,.family="fetch",.reason=M8_THUMB_REASON_FETCH,.fp_prepared=fp_prepared};
    arch->last_executed=arch->last_special=0;
    last_boundary=M8_THUMB_TB_CONTINUE;
    int status=1;
    uint8_t bytes[2];
    if ((t.pc&1) || !fetch || fetch(ctx,t.pc,2,bytes)) goto done;
    uint32_t h=m8_le_read(bytes,2);
    t.width=(h&0xf800)>=0xe800?4:2;t.w=t.width==4?h<<16:h;t.next=t.pc+t.width;
    if(t.width==4) {
        if(fetch(ctx,t.pc+2,2,bytes))goto done;
        t.w=(h<<16)|m8_le_read(bytes,2);
    }
    t.reason=M8_THUMB_REASON_NONE;
    last_boundary=m8_thumb_decode_boundary(t.w,t.width,arch->control,arch->ipsr);
    /* BKPT executes irrespective of the current IT predicate. */
    if(t.in_it && !(t.width==2 && (h&0xff00)==0xbe00) &&
       !m8_condition(cpu->xpsr,(arch->itstate>>4)+1)) {
        status=0;
    } else {
        status=t.width==2?thumb16(&t,h):thumb32(&t);
        if(!status) { arch->last_executed=1;arch->last_special=(uint32_t)t.special; }
    }
    if(!status) {
        cpu->r[15]=t.next;
        if(t.in_it)m8_native_advance_it(cpu,arch);
        m8_native_save_sp(cpu,arch);
    } else if(status==1)m8_native_save_sp(cpu,arch);
done:
    if(status==2 && !t.reason)t.reason=M8_THUMB_REASON_UNSUPPORTED;
    last_result=(m8_thumb_result){t.pc,t.w,t.width,status,t.family,t.reason,t.address,t.size,t.write,t.next};
    return status;
}

int m8_thumb_interpret_fetch(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch,
                            m8_thumb_fetch_fn fetch, void *ctx) {
    return interpret(cpu,mem,arch,fetch,ctx,0);
}

int m8_thumb_interpret_prepared(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch,
                              m8_thumb_fetch_fn fetch, void *ctx) {
    return interpret(cpu,mem,arch,fetch?fetch:mapped_fetch,fetch?ctx:mem,1);
}

int m8_thumb_interpret(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch) {
    return m8_thumb_interpret_fetch(cpu,mem,arch,mapped_fetch,mem);
}
