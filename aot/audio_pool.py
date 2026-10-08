#!/usr/bin/env python3
"""Add default-off exact audio-pool entries to a generated AOT copy."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

HERE = Path(__file__).resolve().parent
ENTRIES = (0x2ac10, 0x2adbc, 0x2ae28, 0x3ad10)
BODIES = {
    0x2ac10: (0x2ad82, '72dfb5386fbdd80ec5e8dae5ca817ec22c1a864fbd2c4149c2a891000f2d5297'),
    0x2adbc: (0x2ae20, 'a779f89e85297eee4a6130a76667d49cb455dc24ae788fdaa76fe9101da1312c'),
    0x2ae28: (0x2ae6e, '4e28045620d311d2e31c41c1916baaedab1da108f6f8d45b9bdee3be791cc7a4'),
    0x3ad10: (0x3adb4, '783bdaa2809f3b6e9d1918bc31b719f041ebdc1c9a91f7b59a6cfaf6155d9d7f'),
}
LITERALS = {
    0x2ad84: bytes.fromhex('2c92012072b4032028920120ecfeffff74b4032034920120389201203c9201204092012044920120489201204c9201208036010050920120'),
    0x2ae20: bytes.fromhex('2c92012072b40320'),
}
FUNCTION = re.compile(r'^static int fn_([0-9a-f]+)\(m8_cpu \*restrict output,m8_mem \*restrict mem\) \{\n', re.M)


def transform(source):
    # Pin the original instruction inventory, including literal-load PCs.
    # Native admission separately checks code bytes and invalidates the AOT
    # library after writes to executable backing. No per-call byte scan is
    # needed in the integer kernels.
    instructions = {}
    for pc, text in re.findall(r'/\* ([0-9a-f]{8}): (.*?) \*/', source):
        pc = int(pc, 16)
        if any(start <= pc < end for start, (end, _) in BODIES.items()):
            if pc in instructions and instructions[pc] != text:
                raise ValueError(f'inconsistent original instruction at {pc:#x}')
            instructions[pc] = text
    for start, (end, expected) in BODIES.items():
        body = ''.join(f'{pc:08x}: {text}\n' for pc, text in sorted(instructions.items())
                       if start <= pc < end)
        actual = hashlib.sha256(body.encode()).hexdigest()
        if actual != expected:
            raise ValueError(f'audio-pool original body mismatch at {start:#x}: expected={expected} actual={actual}')
    seen = set()
    def replace(match):
        pc = int(match[1], 16)
        if pc not in ENTRIES:
            return match[0]
        seen.add(pc)
        call = ('int pool_result=m8_audio_pool_memset(output,mem);\n'
                'if(pool_result) return M8_AOT_EXIT;\n' if pc == 0x3ad10 else
                'int pool_result=m8_audio_pool_control(output,mem);\n'
                'if(pool_result) return pool_result==3 ? M8_AOT_FAULT : M8_AOT_EXIT;\n')
        return match[0] + '#ifdef M8_AOT_AUDIO_POOL\n' + call + '#endif\n'
    result = FUNCTION.sub(replace, source)
    if seen != set(ENTRIES):
        raise ValueError(f'audio-pool function shape missing: {sorted(set(ENTRIES)-seen)}')
    # Diagnostic/count copies always retain the original instruction path.
    includes = '#include "audio_pool.h"\n#include "audio_pool_memset.h"\n'
    at = result.index('\n', result.index('#include "runtime.h"')) + 1
    verifier = 'int m8_aot_verify_memory(m8_mem *mem,uint32_t *bad_pc) {'
    if result.count(verifier) != 1:
        raise ValueError('original AOT verifier shape is ambiguous')
    result = result.replace(verifier,
        'static int m8_audio_pool_base_verify_memory(m8_mem *mem,uint32_t *bad_pc) {', 1)
    checks = ['\nint m8_aot_verify_memory(m8_mem *mem,uint32_t *bad_pc) {',
              'if(m8_audio_pool_base_verify_memory(mem,bad_pc)) return 1;']
    for pc, data in LITERALS.items():
        checks += ['{static const uint8_t expected[]={' + ','.join(map(str, data)) + '};',
                   f'uint8_t *p=m8_pointer(mem,0x{pc:x}u,sizeof expected);',
                   f'if(!p || memcmp(p,expected,sizeof expected)) {{*bad_pc=0x{pc:x}u; return 1;}}}}']
    checks += ['return 0;', '}']
    marker = '\n'.join(checks) + '\nuint32_t m8_aot_audio_pool_enabled(void) {\n'
    marker += '#if defined(M8_AOT_AUDIO_POOL) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)\nreturn 1;\n#else\nreturn 0;\n#endif\n}\n'
    return result[:at] + includes + result[at:] + marker


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    source = args.input.read_text()
    result = transform(source)
    args.output.mkdir(parents=True, exist_ok=True)
    # Preserve guarded and specialized headers from the input bundle. New
    # pool helpers are separate from the maintained translator and voices.
    for header in args.input.parent.glob('*.h'):
        shutil.copyfile(header, args.output / header.name)
    for name in ('audio_pool.h', 'audio_pool_memset.h'):
        shutil.copyfile(HERE / name, args.output / name)
    (args.output / 'generated.c').write_text(result)
    (args.output / 'manifest.json').write_text(json.dumps({
        'input_sha256': hashlib.sha256(source.encode()).hexdigest(),
        'output_sha256': hashlib.sha256(result.encode()).hexdigest(),
        'entries': [hex(pc) for pc in ENTRIES],
        'option': 'M8_AOT_AUDIO_POOL',
        'scope': 'exact qualified data-RAM entries; original code on decline',
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
