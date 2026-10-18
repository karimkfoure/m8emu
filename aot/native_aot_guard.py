#!/usr/bin/env python3
"""Copy a generated AOT library with data-only native store admission."""
import argparse
from bisect import bisect_right
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
INSTRUCTION = re.compile(r'/\* ([0-9a-fA-F]{8}): ([\w.]+)(?: [^*]*)?\*/\Z')
CALL = re.compile(r'\b(m8_write_plain|m8_write)\s*\(')
POINTERS = {
    'M8_FAST_POINTER': 'm8_aot_guard_data_pointer',
    'M8_FAST_SPAN_POINTER': 'm8_aot_guard_data_pointer',
    'm8_pointer': 'm8_aot_guard_data_pointer',
    'm8_span_pointer': 'm8_aot_guard_data_pointer',
    'm8_canonical_pointer': 'm8_aot_guard_data_pointer',
    'm8_canonical_offset_valid': 'm8_aot_guard_offset_data_valid',
    'm8_canonical_offset_pointer': 'm8_aot_guard_offset_data_pointer',
}
STORE = ('str', 'stm', 'push', 'vstr', 'vstm', 'vpush')
FUNCTION = re.compile(r'^static int (fn_[^;\n]+)\{\n', re.M)
HOISTED = re.compile(r'\b(\w+)\s*=\s*(' + '|'.join(POINTERS) + r')\s*(?=\()')
BASE_HEADERS = {'runtime.h', 'memory_spans.h', 'canonical_memory.h',
                'm8_aot.h', 'integer.h', 'fp.h', 'local_flags.h'}
# This opt-in voice qualifies every writable span as DTCM/OCRAM, while
# admitting separate read-only literals and method tables. Rewriting its
# shared pointer helper would reject those reads and disable the kernel.
# Freeze the complete admission proof; any change requires a new audit.
SELF_GUARDED_HEADERS = {
    'oscillator_voice.h': '9c1f6891ae6bd205ea426b664ddddd4ba18b790b9b92ed0ffb6e97350480f911',
}

HEADER = r'''#ifndef M8_NATIVE_AOT_GUARD_H
#define M8_NATIVE_AOT_GUARD_H
#include "runtime.h"
#include "memory_spans.h"
#include "canonical_memory.h"
#if defined(M8_AOT_COMPACT_TABLE) && M8_AOT_COMPACT_TABLE && defined(M8_AOT_COMPACT_MEMORY)
#include "compact_memory.h"
#endif

static inline uint8_t *m8_aot_guard_offset_data_pointer(uintptr_t base,
                                                       uint32_t addr,uint32_t n);
static inline int m8_aot_guard_host_overlap(const uint8_t *p,uint32_t n,
                                           const m8_region *r) {
    if(!n || !r->host || !r->size) return 0;
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)r->host;
    if(a>UINTPTR_MAX-n || b>UINTPTR_MAX-r->size) return 1;
    return a<b+r->size && b<a+n;
}
/* Keep the original span priority and reject aliases of executable backing. */
#if defined(M8_AOT_COMPACT_TABLE) && M8_AOT_COMPACT_TABLE && defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
M8_MEM_INLINE
#else
static inline
#endif
uint8_t *m8_aot_guard_data_pointer(m8_mem *mem,uint32_t addr,
                                                uint32_t n) {
#if defined(M8_AOT_FIXED_MEMORY) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    return m8_aot_guard_offset_data_pointer((uintptr_t)mem->itcm.host,addr,n);
#elif defined(M8_AOT_COMPACT_TABLE) && M8_AOT_COMPACT_TABLE && defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    /* Public admission proves disjoint, stable compact backing. The data
     * table keeps the complete span check and rejects executable regions. */
    return m8_compact_data_pointer(mem,addr,n);
#else
    uint8_t *p=m8_span_pointer(mem,addr,n),*data=NULL;
    if(!p) return NULL;
    data=m8_region_pointer(&mem->dtcm,addr,n);
    if(!data) data=m8_region_pointer(&mem->ocram,addr,n);
    if(p!=data || m8_aot_guard_host_overlap(p,n,&mem->itcm) ||
       m8_aot_guard_host_overlap(p,n,&mem->flash)) return NULL;
    return p;
#endif
}
#if defined(M8_AOT_FIXED_MEMORY) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
M8_MEM_INLINE
#else
static inline
#endif
uint8_t *m8_aot_guard_region_data_pointer(m8_mem *mem,
                                                       m8_region *r,
                                                       uint32_t addr,uint32_t n) {
#if defined(M8_AOT_FIXED_MEMORY) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    /* Public admission proves canonical metadata and fixed backing, stable
     * through callbacks. A known data region needs only its own span bound;
     * the second map lookup and host-pointer comparison prove the same fact.
     * Keep arbitrary descriptors on the original ordered admission path. */
    if(r==&mem->dtcm || r==&mem->ocram) {
        uint32_t base=r==&mem->dtcm ? 0x20000000u : 0x20200000u;
        return n<=0x80000u && (uint32_t)(addr-base)<=0x80000u-n ?
            (uint8_t *)((uintptr_t)mem->itcm.host+(uintptr_t)addr) : NULL;
    }
#endif
    uint8_t *p=m8_region_pointer(r,addr,n);
    return p && p==m8_aot_guard_data_pointer(mem,addr,n) ? p : NULL;
}
/* The caller already qualified the canonical fixed-offset map. */
static inline int m8_aot_guard_offset_data_valid(uint32_t addr,uint32_t n) {
    return n<=0x80000u &&
        ((uint32_t)(addr-0x20000000u)<=0x80000u-n ||
         (uint32_t)(addr-0x20200000u)<=0x80000u-n);
}
static inline uint8_t *m8_aot_guard_offset_data_pointer(uintptr_t base,
                                                       uint32_t addr,uint32_t n) {
    return m8_aot_guard_offset_data_valid(addr,n) ?
        (uint8_t *)(base+(uintptr_t)addr) : NULL;
}
static inline int m8_aot_guard_guest_overlap(uint32_t addr,uint32_t n,
                                            const m8_region *r) {
    uint64_t end=(uint64_t)addr+n,limit=(uint64_t)r->base+r->size;
    return n && r->size && (uint64_t)addr<limit && (uint64_t)r->base<end;
}
static inline int m8_aot_guard_writable(m8_mem *mem,uint32_t addr,uint32_t n) {
    if((uint64_t)addr+n>UINT64_C(0x100000000)) return 0;
    uint8_t *p=m8_pointer(mem,addr,n);
    if(p) return m8_aot_guard_data_pointer(mem,addr,n)!=NULL;
    return !m8_aot_guard_guest_overlap(addr,n,&mem->itcm) &&
           !m8_aot_guard_guest_overlap(addr,n,&mem->flash);
}
static inline int m8_aot_guard_write(m8_mem *mem,uint32_t addr,unsigned n,
                                     uint32_t value,int *slow,int *declined) {
    if(!m8_aot_guard_writable(mem,addr,n)) { *declined=1; return 2; }
    return m8_write(mem,addr,n,value,slow);
}
static inline int m8_aot_guard_write_plain(m8_mem *mem,uint32_t addr,unsigned n,
                                           uint32_t value,int *slow,int *declined) {
    if(!m8_aot_guard_writable(mem,addr,n)) { *declined=1; return 2; }
    return m8_write_plain(mem,addr,n,value,slow);
}
#endif
'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def units(source):
    """Find the lexical instruction scopes emitted by the maintained translator."""
    stack, found = [], []
    for token in TOKEN.finditer(source):
        value = token.group()
        if value == '{':
            stack.append([token.start(), None])
        elif value == '}':
            if not stack:
                raise ValueError('unbalanced generated closing brace')
            start, instruction = stack.pop()
            if instruction:
                found.append((start, token.end(), *instruction))
        elif value.startswith('/*'):
            match = INSTRUCTION.fullmatch(value)
            if match:
                if not stack or stack[-1][1]:
                    raise ValueError(f'instruction scope is ambiguous at {match[1]}')
                stack[-1][1] = (int(match[1], 16), match[2].lower())
    if stack:
        raise ValueError('unbalanced generated opening brace')
    return sorted(found)


KERNEL_READ_CALLS = {
    'fm_batch.h': ('m8_pointer(mem,0x143f8u,64)',
                   'm8_pointer(mem,callback_address,4)',
                   'm8_pointer(mem,m8_le_read(callback,4),4)'),
    'braids_loop.h': ('m8_span_pointer(mem,0x60015c50u,8u)',
                      'm8_span_pointer(mem,begin,count)',
                      'm8_span_pointer(mem,table1_address,514u)'),
    'braids_mode0.h': ('m8_pointer(mem,0x2294u,4u)',
                       'm8_pointer(mem,0x36d90u,16u)',
                       'm8_pointer(mem,m8_filter_read32(sine_literal+8u),513u*4u)',
                       'm8_pointer(mem,0x19814u,24u)'),
    'braids_filter.h': ('m8_pointer(mem,callback_address,4u)',
                        'm8_pointer(mem,m8_filter_read32(callback),4u)',
                        'm8_pointer(mem,0x19900u,28u)'),
}
KERNEL_POINTER_COUNTS = {'fm_batch.h': 3, 'braids_loop.h': 7, 'braids_filter.h': 9, 'braids_mode0.h': 12}


def rename_pointers(text):
    return re.sub(r'\b(' + '|'.join(POINTERS) + r')\s*(?=\()',
                  lambda m: POINTERS[m[1]], text)


def guarded_writes(text):
    edits = []
    for match in CALL.finditer(text):
        start, depth, end = match.end(), 1, match.end()
        while depth and end < len(text):
            depth += (text[end] == '(') - (text[end] == ')')
            end += 1
        if depth:
            raise ValueError('unbalanced store callback arguments')
        edits.append((match.start(), end, 'm8_aot_guard_' + match[1][3:] +
                      '(' + text[start:end - 1] + ',&m8_aot_guard_declined)'))
    for start, end, replacement in reversed(edits):
        text = text[:start] + replacement + text[end:]
    return text, len(edits)


def write_pointer_ids(body):
    """Trace direct store destinations through local generated pointer aliases."""
    aliases = {}
    for assignment in re.finditer(r'\b(\w+)\s*=(?!=)\s*([^;\n]+);', body):
        aliases.setdefault(assignment[1], set()).update(
            re.findall(r'\b\w+\b', assignment[2]))
    targets = set()
    for call in re.finditer(r'\bm8_(?:le_write|write_it)\s*\(', body):
        at, depth = call.end(), 0
        end = at
        while end < len(body):
            character = body[end]
            if character == ',' and depth == 0:
                break
            depth += (character == '(') - (character == ')')
            if depth < 0:
                raise ValueError('direct store destination shape changed')
            end += 1
        else:
            raise ValueError('unbalanced direct store destination')
        targets.update(re.findall(r'\b\w+\b', body[at:end]))
    pending = list(targets)
    while pending:
        for dependency in aliases.get(pending.pop(), ()):
            if dependency not in targets:
                targets.add(dependency)
                pending.append(dependency)
    return targets


def hoisted(text, offset, function_starts, writes, qualifications):
    """Guard cached store pointers without restricting cached code reads."""
    def qualify(match):
        index = bisect_right(function_starts, offset + match.start()) - 1
        guarded = index >= 0 and match[1] in writes[index]
        qualifications['write' if guarded else 'read_only'] += 1
        return match[0].replace(match[2], POINTERS[match[2]]) if guarded else match[0]
    return HOISTED.sub(qualify, text)


def transform(source):
    scopes = units(source)
    if not scopes:
        raise ValueError('generated source has no original instruction scopes')
    function_starts = [match.start() for match in FUNCTION.finditer(source)]
    writes = [set() for _ in function_starts]
    for start, end, pc, mnemonic in scopes:
        if mnemonic.startswith(STORE):
            index = bisect_right(function_starts, start) - 1
            if index < 0:
                raise ValueError(f'store scope outside generated function at {pc:x}')
            writes[index].update(write_pointer_ids(source[start:end]))
    qualifications = {'write': 0, 'read_only': 0}
    parts, at, stores, callbacks, unclassified = [], 0, 0, 0, []
    for start, end, pc, mnemonic in scopes:
        outside = source[at:start]
        if re.search(r'\bm8_(?:le_write|write_it|write_plain|write)\s*\(', outside):
            unclassified.append({'before_pc': pc, 'reason': 'store outside original instruction scope'})
        parts.append(hoisted(outside, at, function_starts, writes, qualifications))
        body = source[start:end]
        if mnemonic.startswith(STORE):
            body = rename_pointers(body)
            body, count = guarded_writes(body)
            callbacks += count
            stores += 1
        elif re.search(r'\bm8_(?:le_write|write_it|write_plain|write)\s*\(', body):
            unclassified.append({'pc': pc, 'mnemonic': mnemonic,
                                 'reason': 'store in unclassified instruction'})
        parts.append(body)
        at = end
    tail = source[at:]
    if re.search(r'\bm8_(?:le_write|write_it|write_plain|write)\s*\(', tail):
        unclassified.append({'reason': 'store after final instruction scope'})
    parts.append(hoisted(tail, at, function_starts, writes, qualifications))
    if unclassified:
        raise ValueError('unclassified write paths: ' + json.dumps(unclassified[:12]))
    result = ''.join(parts)
    function = re.compile(r'(static int fn_[^;\n]+\{\n)')
    result, functions = function.subn(r'\1int m8_aot_guard_declined=0;\n', result)
    result, faults = re.subn(r'fault:\nM8_COUNT_REWIND\(\);',
                            'fault:\nif(m8_aot_guard_declined) {r_15=pc; goto memory_fallback;}\nM8_COUNT_REWIND();', result)
    if not functions or functions != faults:
        raise ValueError(f'fault publication shape changed: functions={functions} faults={faults}')
    result = '#include "native_aot_guard.h"\n' + result
    result += '\nuint32_t m8_aot_native_code_guard(void) {return 1;}\n'
    return result, {'instruction_scopes': len(scopes), 'store_scopes': stores,
                    'guarded_generic_writes': callbacks, 'guarded_functions': functions,
                    'cached_write_qualifications': qualifications['write'],
                    'cached_read_only_qualifications': qualifications['read_only'],
                    'rejected_write_paths': []}


def copy_headers(source, header_dir):
    needed = set(re.findall(r'^#include "([^"/]+)"', source, flags=re.M))
    needed.update(('runtime.h', 'memory_spans.h', 'canonical_memory.h'))
    needed.discard('native_aot_guard.h')
    outputs = {}
    while needed:
        name = min(needed)
        needed.remove(name)
        path = header_dir / name
        if name in outputs:
            continue
        if not path.is_file():
            raise ValueError(f'missing local AOT header: {path}')
        text = path.read_text()
        needed.update(set(re.findall(r'^#include "([^"/]+)"', text, flags=re.M)) - set(outputs))
        if name in SELF_GUARDED_HEADERS:
            if digest(text.encode()) != SELF_GUARDED_HEADERS[name]:
                raise ValueError(f'{name}: self-guarded admission proof changed')
        elif name not in BASE_HEADERS:
            # Protect only audited read qualifications from the conservative
            # kernel rewrite. Full destination admission and existing alias
            # checks remain before effects. Refuse changed call sets.
            reads = KERNEL_READ_CALLS.get(name, ())
            if reads:
                calls = len(re.findall(r'\bm8_(?:pointer|span_pointer)\s*\(', text))
                if any(text.count(call) != 1 for call in reads) or calls != KERNEL_POINTER_COUNTS[name]:
                    raise ValueError(f'{name}: read-only pointer qualification set changed')
                for index, call in enumerate(reads):
                    text = text.replace(call, f'M8_AOT_GUARD_READ_{index}')
            text = rename_pointers(text)
            for index, call in enumerate(reads):
                text = text.replace(f'M8_AOT_GUARD_READ_{index}', call)
            text = re.sub(r'\bm8_region_pointer\((&mem->(?:dtcm|ocram)),',
                          r'm8_aot_guard_region_data_pointer(mem,\1,', text)
            if name == 'memcpy_kernel.h':
                old = 'uint8_t *to=m8_copy_ram(mem,dst,size),*from=m8_copy_ram(mem,src,size);'
                new = 'uint8_t *to=m8_aot_guard_data_pointer(mem,dst,size),*from=m8_copy_ram(mem,src,size);'
                if text.count(old) != 1:
                    raise ValueError('memcpy kernel destination preflight changed')
                text = text.replace(old, new)
        outputs[name] = text
    return outputs


def retired_headers(output, current):
    """Validate previously owned headers before removing them on replacement."""
    manifest = output / 'manifest.json'
    if not manifest.exists():
        return []
    previous = json.loads(manifest.read_text())
    if not isinstance(previous, dict):
        raise ValueError(f'first_false=previous manifest object path={manifest} actual_type={type(previous).__name__}')
    if previous.get('schema') != 'm8-native-aot-guard/1':
        raise ValueError(f'first_false=previous guard schema path={manifest} expected=m8-native-aot-guard/1 actual={previous.get("schema")!r}')
    if not isinstance(previous.get('headers'), dict):
        raise ValueError(f'first_false=previous headers object path={manifest} actual_type={type(previous.get("headers")).__name__}')
    retired = []
    for name in sorted(previous['headers'].keys() - current.keys()):
        if Path(name).name != name or name in ('.', '..'):
            raise ValueError(f'first_false=retired header basename name={name!r} basename={Path(name).name!r}')
        path = output / name
        if path.is_symlink():
            raise ValueError(f'first_false=retired header is not a symlink path={path} is_symlink=True')
        if not path.exists():
            continue
        if not path.is_file():
            raise ValueError(f'first_false=retired header is a regular file path={path} is_file=False')
        expected = previous['headers'][name]['output_sha256']
        actual = digest(path.read_bytes())
        if actual != expected:
            raise ValueError(f'first_false=retired header SHA256 path={path} expected={expected} actual={actual}')
        retired.append(path)
    return retired


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--headers', type=Path, default=HERE)
    args = parser.parse_args()
    if args.output.resolve() == args.input.resolve().parent or args.output.resolve() == args.headers.resolve():
        parser.error('output must be separate from the original source and headers')
    original = args.input.read_bytes()
    try:
        guarded, changes = transform(original.decode())
        headers = copy_headers(guarded, args.headers)
        retired = retired_headers(args.output, headers)
    except ValueError as error:
        parser.error(str(error))
    args.output.mkdir(parents=True, exist_ok=True)
    for path in retired:
        path.unlink()
    (args.output / 'generated.c').write_text(guarded)
    (args.output / 'native_aot_guard.h').write_text(HEADER)
    identities = {}
    for name, text in sorted(headers.items()):
        (args.output / name).write_text(text)
        identities[name] = {'input_sha256': digest((args.headers / name).read_bytes()),
                            'output_sha256': digest(text.encode()),
                            'kernel_spans_data_only': name not in BASE_HEADERS and name not in KERNEL_READ_CALLS and name not in SELF_GUARDED_HEADERS,
                            'preserved_read_calls': list(KERNEL_READ_CALLS.get(name, ()))}
    manifest = {'schema': 'm8-native-aot-guard/1',
                'input': str(args.input.resolve()), 'input_sha256': digest(original),
                'output_sha256': digest(guarded.encode()),
                'guard_header_sha256': digest(HEADER.encode()),
                'transformations': changes, 'headers': identities,
                'limits': ['Candidate only; code writes decline the AOT operation.',
                           'Fixed-offset stores require the existing fixed-map admission.',
                           'FM batch, Braids loop and Braids filter preserve audited code reads; complete write spans remain restricted to data RAM.',
                           'Other copied kernels still decline executable input spans before effects.',
                           'Runtime must disable native translations and hand off the published pre-store state to Unicorn without flushing its cached block.']}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Guarded {changes["store_scopes"]} store scopes; {changes["guarded_functions"]} functions; {len(headers)} copied headers.')


if __name__ == '__main__':
    main()
