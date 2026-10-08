"""Qualify whole RAM spans through function-invariant guest integer bases."""
import ast
from dataclasses import dataclass
import re
from capstone import arm as A

ACCESS = re.compile(r'if \(m8_(read|write)\(mem, (.*), (\d+), (.*), &slow\)\) goto fault;')
ASSIGN = re.compile(r'cpu->r\[(\d+)\]\s*(?:=|\+=|-=|\|=|&=|\^=)')
CALL = re.compile(r'fn_([0-9a-f]+)\(cpu,mem\)')
ALL = set(range(16))


def base_writes(translator):
    """Summarize emitted writes and calls, independently of publication masks.

    Local computed branches do not change integer bases. An unknown BLX can
    return with any base changed; an unknown tail return cannot resume here.
    Unsupported instructions exit before their effects and are not writes.
    """
    writes, children = {}, {}
    for function in translator.functions:
        modified, calls = set(), set()
        for unit in function.units.values():
            for insn in unit.instructions:
                body = translator.instruction(function, insn)
                if body is None or (unit.it and insn.id in (A.ARM_INS_BL, A.ARM_INS_BLX)):
                    continue
                source = '\n'.join(body)
                modified.update(int(n) for n in ASSIGN.findall(source))
                calls.update(int(n, 16) for n in CALL.findall(source))
                if insn.id == A.ARM_INS_BLX:
                    modified.update(ALL)
        writes[function.entry], children[function.entry] = modified, calls
    changed = True
    while changed:
        changed = False
        for entry, calls in children.items():
            previous = len(writes[entry])
            for callee in calls:
                writes[entry].update(writes.get(callee, ALL))
            changed |= previous != len(writes[entry])
    return writes


def affine(expression, variables):
    """Parse only constants and one guest base; reject indexed expressions."""
    expression = re.sub(r'cpu->r\[(\d+)\]', r'R_\1', expression)
    expression = re.sub(r'UINT32_C\(([^)]+)\)', r'(\1)', expression)
    expression = re.sub(r'\b(0x[0-9a-fA-F]+|\d+)[uUlL]+\b', r'\1', expression)
    def visit(node):
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return None, node.value
        if isinstance(node, ast.Name):
            if node.id.startswith('R_') and node.id[2:].isdigit():
                return int(node.id[2:]), 0
            return variables.get(node.id)
        if isinstance(node, ast.UnaryOp):
            value = visit(node.operand)
            if value is None or value[0] is not None:
                return None
            if isinstance(node.op, ast.USub): return None, -value[1]
            if isinstance(node.op, ast.Invert): return None, ~value[1]
        if isinstance(node, ast.BinOp):
            left, right = visit(node.left), visit(node.right)
            if left is None or right is None: return None
            if isinstance(node.op, (ast.Add, ast.Sub)):
                sign = -1 if isinstance(node.op, ast.Sub) else 1
                if right[0] is not None and (left[0] is not None or sign < 0): return None
                return left[0] if left[0] is not None else right[0], left[1] + sign * right[1]
            if isinstance(node.op, ast.BitAnd) and left[0] is None and right[0] is None:
                return None, left[1] & right[1]
        return None
    try:
        return visit(ast.parse(expression, mode='eval').body)
    except (SyntaxError, ValueError):
        return None


@dataclass
class Plan:
    groups: list
    accesses: dict

    def uninterrupted_it(self, unit, bodies):
        """All effects stay native after entry has qualified these RAM spans."""
        if not unit.it:
            return False
        accesses = 0
        for insn, body in zip(unit.instructions, bodies):
            if body is None:
                return False
            for line in body:
                if 'm8_read(' in line or 'm8_write(' in line:
                    if ACCESS.fullmatch(line) is None or line not in self.accesses.get(insn.address, {}):
                        return False
                    accesses += 1
                    continue
                # Calls, PC writes and branches can cross an unknown boundary.
                if any(token in line for token in ('goto ', 'return ', 'fn_', 'm8_tail(')):
                    return False
        return accesses > 0

    def prelude(self):
        rows = ['#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC)']
        for index, group in enumerate(self.groups):
            base, low, high = group['base'], group['low'], group['high']
            value = '0' if base is None else f'output->r[{base}]'
            rows += [f'int64_t span_start_{index}=(int64_t)(uint64_t)({value})+({low});',
                     f'int64_t span_end_{index}=(int64_t)(uint64_t)({value})+({high});',
                     f'if(span_start_{index}<0 || span_end_{index}>INT64_C(0x100000000)) return M8_AOT_EXIT;',
                     f'uint8_t *span_host_{index}=M8_FAST_SPAN_POINTER(mem,(uint32_t)span_start_{index},{high-low}u);',
                     f'if(!span_host_{index}) return M8_AOT_EXIT;']
        return '\n'.join(rows + ['#endif', ''])

    def replace(self, pc, body, it):
        replacements = self.accesses.get(pc)
        if not replacements: return None
        result = []
        for line in body:
            match = ACCESS.fullmatch(line)
            if match is None:
                result.append(line)
                continue
            index, offset = replacements[line]
            host = f'(span_host_{index}+{offset}u)'
            operation, _address, size, value = match.groups()
            if operation == 'read':
                result.append(f'{value.removeprefix("&")}=m8_le_read({host},{size});')
            elif it:
                result.append(f'if(m8_write_it({host},{size},{value})) goto fault;')
            else:
                result.append(f'm8_le_write({host},{size},{value});')
        return result

    def stats(self):
        return {'spans': self.groups, 'accesses': sum(len(a) for a in self.accesses.values()),
                'uninterrupted_it_units': []}


def analyze(function, bodies, modified):
    # These functions can accept NVIC words through m8rt's cooperative service.
    # A direct-region precondition must not disable that continuation contract.
    if function.entry in (0x2ac10, 0x2adbc, 0x2542c, 0x1bc58, 0x6001dbf8):
        return Plan([], {})
    candidates, uses = {}, {}
    for pc, unit in function.units.items():
        if any(body is None for body in bodies[pc]): continue
        for insn, body in zip(unit.instructions, bodies[pc]):
            if body is None: continue
            # These service-backed accesses have a separate host contract.
            if function.entry == 0x31680 and pc in (0x31688, 0x31694, 0x3169a, 0x316b8): continue
            if function.entry == 0x34848 and pc == 0x348b2: continue
            variables, accesses = {}, []
            valid = True
            for line in body:
                declaration = re.fullmatch(r'uint32_t (base|addr|fp_addr) = (.*);', line)
                if declaration:
                    variables[declaration[1]] = affine(declaration[2], variables)
                match = ACCESS.fullmatch(line)
                if match is None: continue
                value = affine(match[2], variables)
                if value is None or (value[0] is not None and value[0] in modified):
                    valid = False
                    break
                base, offset = value
                offset &= 0xffffffff
                if base is not None and offset >= 0x80000000: offset -= 0x100000000
                if base is None and offset + int(match[3]) > 0x100000000:
                    valid = False
                    break
                accesses.append((base, offset, int(match[3]), line))
            if not valid or not accesses: continue
            candidates[insn.address] = accesses
            for base, offset, size, _line in accesses:
                uses.setdefault(base, []).append((offset, offset+size))
    groups, indices = [], {}
    for base, ranges in sorted(uses.items(), key=lambda item: -1 if item[0] is None else item[0]):
        low, high = min(r[0] for r in ranges), max(r[1] for r in ranges)
        if len(ranges) < 2 or high-low > 0xffffffff: continue
        indices[base] = len(groups)
        groups.append({'base': base, 'low': low, 'high': high, 'accesses': len(ranges)})
    qualified = {}
    for pc, accesses in candidates.items():
        if not all(base in indices for base, _offset, _size, _line in accesses): continue
        qualified[pc] = {line: (indices[base], offset-groups[indices[base]]['low'])
                         for base, offset, _size, line in accesses}
    return Plan(groups, qualified)
