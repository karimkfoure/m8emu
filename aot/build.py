#!/usr/bin/env python3
"""Regenerate and compile the portable AOT library."""
import argparse
import os
import platform
from pathlib import Path
import subprocess
import sys
import tempfile
HERE=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--functions',type=Path,default=HERE/'functions.txt')
    p.add_argument('--function-fragment',type=Path,action='append',default=[],help='Append original function bounds from an additional catalog')
    p.add_argument('--hooks',type=Path,default=HERE/'enabled.txt')
    p.add_argument('--entry-hooks',type=Path,help='Limit installed hooks to selected safe roots/continuations')
    p.add_argument('--firmware',type=Path)
    p.add_argument('--cc',default=os.environ.get('CC','cc'))
    p.add_argument('--common-fp',action='store_true',help='Use guarded common-mode FP operations')
    p.add_argument('--native-fp',action='store_true',help='Use native arithmetic for the firmware RN/FZ/DN mode')
    p.add_argument('--int-float-scale',action='store_true',help='Use direct native F32 fixed-point conversion scaling')
    p.add_argument('--fast-memory',action='store_true',help='Use direct RAM access and return to Unicorn before non-RAM instructions')
    p.add_argument('--fixed-memory',action='store_true',help='Require guarded fixed-offset m8rt backing and use bounded offset accesses')
    p.add_argument('--validity-first',action='store_true',help='Check fixed scalar spans before forming host pointers')
    p.add_argument('--narrow-calls',action='store_true',help='Copy only referenced registers at known native calls')
    p.add_argument('--hyper-voice',action='store_true',help='Batch bounded unfiltered HYPERSYNTH voice blocks with original control updates')
    p.add_argument('--sampler-voice',action='store_true',help='Batch bounded mono16 unfiltered SAMPLER voice blocks with original control updates')
    p.add_argument('--sampler-loops',action='store_true',help='Permit bounded forward loops in the SAMPLER voice kernel; retain scalar wrap continuations')
    p.add_argument('--dead-nzcv',action='store_true',help='Omit provably dead whole NZCV writes; diagnostics keep every write')
    p.add_argument('--memory-spans',action='store_true',help='Qualify complete spans through invariant bases; diagnostics keep original accesses')
    p.add_argument('--region-spans',action='store_true',help='Qualify stable object spans in bounded call-free regions')
    p.add_argument('--pointee-spans',action='store_true',help='Cache checked memory pointers inside bounded call-free regions')
    p.add_argument('--braids-mode0',action='store_true',help='Use the experimental bounded mode-zero filter quantum')
    p.add_argument('--local-nzcv',action='store_true',help='Keep NZCV values in locals and pack only at state boundaries')
    p.add_argument('--it-preflight',action='store_true',help='Qualify invariant IT-unit spans before effects; diagnostics retain rollback')
    p.add_argument('--call-dispatch',action='store_true',help='Use one exact tagged lookup for computed native calls')
    p.add_argument('--exact-entry-leaves',action='store_true',help='Use experimental exact-entry paths for small register-only native leaves')
    p.add_argument('--dwt-ram',action='store_true',help='Permit non-mutating DWT RAM reads within native calls (m8rt clock contract)')
    p.add_argument('--cooperative-scs',action='store_true',help='Allow qualified NVIC word accesses to continue when m8rt has no required service')
    p.add_argument('--fma',choices=('auto','on','off'),default='auto',help='Enable x86 FMA only for a compatible target and host')
    p.add_argument('--local-cpu',action='store_true',help='Compatibility option; scalar register locals are always enabled')
    p.add_argument('--guest-count',action='store_true',help='Count original native instructions; disable loop specializations')
    p.add_argument('--diagnostic',action='store_true',help='Enable safe instruction-prefix stops')
    p.add_argument('--optimization',choices=('O2','O3'),default='O2',help='Select the compiler optimization level')
    p.add_argument('--output',type=Path,default=HERE/'libm8aot.so')
    args=p.parse_args()
    if args.hyper_voice and not all((args.native_fp,args.fast_memory,args.fixed_memory)):
        p.error('--hyper-voice requires --native-fp, --fast-memory and --fixed-memory')
    if args.sampler_voice and not all((args.native_fp,args.fast_memory,args.fixed_memory)):
        p.error('--sampler-voice requires --native-fp, --fast-memory and --fixed-memory')
    if args.sampler_loops and not args.sampler_voice:
        p.error('--sampler-loops requires --sampler-voice and its prerequisites')
    if args.validity_first and (not args.fast_memory or not args.fixed_memory):
        p.error('--validity-first requires --fast-memory and --fixed-memory')
    if args.memory_spans and not args.fast_memory:
        p.error('--memory-spans requires --fast-memory')
    if args.region_spans and not args.fast_memory:
        p.error('--region-spans requires --fast-memory')
    if args.pointee_spans and not args.fast_memory:
        p.error('--pointee-spans requires --fast-memory')
    if args.braids_mode0 and (not args.native_fp or not args.fast_memory):
        p.error('--braids-mode0 requires --native-fp and --fast-memory')
    if args.int_float_scale and not args.native_fp:
        p.error('--int-float-scale requires --native-fp')
    if args.it_preflight and not args.fast_memory:
        p.error('--it-preflight requires --fast-memory')
    command=[sys.executable,str(HERE/'translate.py'),'--functions',str(args.functions),'--hooks',str(args.hooks)]
    for fragment in args.function_fragment:
        command.extend(['--function-fragment',str(fragment)])
    if args.firmware:
        command.extend(['--firmware',str(args.firmware)])
    if args.entry_hooks:
        command.extend(['--entry-hooks',str(args.entry_hooks)])
    if args.dead_nzcv:
        command.append('--dead-nzcv')
    if args.memory_spans:
        command.append('--memory-spans')
    if args.local_nzcv:
        command.append('--local-nzcv')
    if args.it_preflight:
        command.append('--it-preflight')
    if args.call_dispatch:
        command.append('--call-dispatch')
    if args.exact_entry_leaves:
        command.append('--exact-entry-leaves')
    subprocess.run(command,check=True)
    defines=['-DM8_AOT_DIAGNOSTIC'] if args.diagnostic else []
    if args.guest_count: defines.append('-DM8_AOT_GUEST_COUNT')
    if args.common_fp: defines.append('-DM8_AOT_COMMON_FP')
    if args.native_fp: defines.append('-DM8_AOT_NATIVE_FP')
    if args.int_float_scale: defines.append('-DM8_AOT_INT_FLOAT_SCALE')
    if args.fast_memory: defines.append('-DM8_AOT_FAST_MEMORY')
    if args.validity_first: defines.append('-DM8_AOT_VALIDITY_FIRST')
    if args.region_spans: defines.append('-DM8_AOT_REGION_SPANS')
    if args.pointee_spans: defines.append('-DM8_AOT_POINTEE_SPANS')
    if args.braids_mode0:
        defines.append('-DM8_AOT_BRAIDS_MODE0')
    if args.fixed_memory:
        if not args.fast_memory:
            p.error('--fixed-memory requires --fast-memory')
        defines.append('-DM8_AOT_FIXED_MEMORY')
    if args.narrow_calls: defines.append('-DM8_AOT_NARROW_CALLS')
    if args.hyper_voice: defines.append('-DM8_AOT_HYPER_VOICE')
    if args.sampler_voice: defines.append('-DM8_AOT_SAMPLER_VOICE')
    if args.sampler_loops: defines.append('-DM8_AOT_SAMPLER_LOOPS')
    if args.dwt_ram:
        if not args.fast_memory:
            p.error('--dwt-ram requires --fast-memory')
        defines.append('-DM8_AOT_DWT_RAM')
    if args.cooperative_scs:
        if not args.fast_memory:
            p.error('--cooperative-scs requires --fast-memory')
        defines.append('-DM8_AOT_COOPERATIVE_SCS')
    target=subprocess.check_output([args.cc,'-dumpmachine'],text=True).strip()
    host_fma=False
    if platform.machine() in ('x86_64','AMD64'):
        try:
            host_fma='fma' in Path('/proc/cpuinfo').read_text().split()
        except OSError:
            pass
    flags=[]
    if args.native_fp:
        flags.append('-fno-math-errno')
    if args.native_fp and target.startswith('x86_64') and (args.fma=='on' or (args.fma=='auto' and host_fma)):
        flags.append('-mfma')
    elif args.fma=='on' and not target.startswith('x86_64'):
        p.error('--fma on requires an x86-64 compiler target')
    args.output=args.output.resolve()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.aot-build-',dir=args.output.parent) as temporary:
        artifact=Path(temporary)/args.output.name
        subprocess.run([args.cc,*defines,*flags,'-std=c11','-'+args.optimization,'-fPIC','-shared','-frounding-math','-ffp-contract=off','-Wall','-Wextra','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-but-set-variable','-I',str(HERE),str(HERE/'generated.c'),'-o',str(artifact),'-lm'],check=True)
        os.replace(artifact,args.output)
    print(args.output)
if __name__=='__main__':
    main()
