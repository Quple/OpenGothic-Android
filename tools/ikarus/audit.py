#!/usr/bin/env python3
"""Translate Daedalus sources and DAT calls to an auditable call-graph IR.

No game or scripts are executed. Counts are static call sites, not runtime hits.
Sources are optional when --dat is supplied. Third-party sources and DAT stay local.
"""
import argparse
import bisect
import collections
import csv
import hashlib
import json
import re
import struct
from pathlib import Path

from disasm import Dat, OPS


# Keep strings atomic: braces, comments and function-looking text inside a string
# must never become declarations/calls. Retain byte offsets for source locations.
LEX = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|_(?:@[sSfF]?|\^)|[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|\d+|[^\s]', re.S)
IDENT = re.compile(r'(?:[A-Za-z_]\w*|_(?:@[sSfF]?|\^))\Z')
CONTROL = {'IF', 'WHILE', 'RETURN', 'ELSE'}
NATIVE = {'CALL__THISCALL', 'CALL__STDCALL', 'CALL__CDECL', 'CALL__FASTCALL'}
HOOK = {'HOOKENGINE', 'HOOKENGINEF', 'HOOKENGINEI'}
IMM = {61, 62, 64, 65, 67, 75, 76, 80, 245}


def tokens(text):
    return [(m.group(), m.start()) for m in LEX.finditer(text)
            if not m.group().startswith(('//', '/*'))]


def pairs(ts):
    stack, out = [], {}
    for i, (t, _) in enumerate(ts):
        if t in ('(', '{', '['):
            stack.append((t, i))
        elif t in (')', '}', ']') and stack:
            opening, j = stack.pop()
            if opening + t in ('()', '{}', '[]'):
                out[j] = i
    return out


def arguments(ts, match, start):
    end = match[start]
    result, i, begin = [], start + 1, start + 1
    while i < end:
        if ts[i][0] == ',':
            result.append(''.join(t for t, _ in ts[begin:i]))
            begin = i + 1
        i = match[i] + 1 if i in match else i + 1
    if begin < end:
        result.append(''.join(t for t, _ in ts[begin:end]))
    return result


def number(expr, constants):
    """Deliberately restricted; never eval source code or guess dynamic pointers."""
    expr = expr.strip()
    switch = re.fullmatch(r'MEMINT_SWITCHG1G2\(([^,]+),([^,]+)\)', expr, re.I)
    if switch:
        return number(switch[2], constants)
    if re.fullmatch(r'-?(?:0[xX][0-9a-fA-F]+|\d+)', expr):
        return int(expr, 16 if 'x' in expr.lower() else 10)
    return constants.get(expr.upper())


def source_ir(roots):
    definitions, calls, constants, manifests = [], [], {}, []
    for origin, root in roots:
        print(f'  {origin}: {root}', flush=True)
        if not root.is_dir():
            raise ValueError(f'Source directory does not exist: {root}')
        for path in sorted(root.rglob('*')):
            if path.suffix.lower() != '.d':
                continue
            raw = path.read_bytes()
            text = raw.decode('latin-1')
            rel = origin + '/' + path.relative_to(root).as_posix()
            manifests.append({'file': rel, 'sha256': hashlib.sha256(raw).hexdigest()})
            ts = tokens(text)
            match = pairs(ts)
            newlines = [m.start() for m in re.finditer('\n', text)]
            loc = lambda offset: bisect.bisect_left(newlines, offset) + 1
            # Declarations can be prototypes (ending in ';'); do not seek the
            # next unrelated '{', which used to attach another function's body.
            names, scopes, file_constants = set(), [], []
            for i in range(len(ts) - 3):
                word = ts[i][0].upper()
                if word == 'CONST' and ts[i+1][0].upper() == 'INT' and ts[i+3][0] == '=':
                    j = i + 4
                    while j < len(ts) and ts[j][0] != ';':
                        j += 1
                    value = number(''.join(t for t, _ in ts[i+4:j]), constants)
                    if value is not None:
                        file_constants.append((i, ts[i+2][0].upper(), value))
                if word not in ('FUNC', 'INSTANCE', 'PROTOTYPE'):
                    continue
                ni = i + (2 if word == 'FUNC' else 1)
                if ni + 1 >= len(ts) or ts[ni+1][0] != '(' or ni+1 not in match:
                    continue
                names.add(ni)
                end = match[ni+1] + 1
                body_end = match.get(end) if end < len(ts) and ts[end][0] == '{' else None
                name = ts[ni][0].upper()
                definitions.append({'name': name, 'kind': word.lower(), 'file': rel,
                                    'line': loc(ts[i][1]), 'body': body_end is not None})
                if body_end is not None:
                    scopes.append((end, body_end, name))
            si = 0
            for i, name, value in file_constants:
                while si < len(scopes) and i > scopes[si][1]:
                    si += 1
                if si < len(scopes) and scopes[si][0] < i < scopes[si][1]:
                    constants[scopes[si][2] + '.' + name] = value
                else:
                    constants[name] = value
            si = 0
            for i in range(len(ts) - 1):
                name = ts[i][0].upper()
                if i in names or not IDENT.fullmatch(ts[i][0]) or name in CONTROL:
                    continue
                if ts[i+1][0] != '(' or i+1 not in match:
                    continue
                while si < len(scopes) and i > scopes[si][1]:
                    si += 1
                caller = scopes[si][2] if si < len(scopes) and scopes[si][0] < i < scopes[si][1] else '<GLOBAL>'
                calls.append({'caller': caller, 'callee': name, 'file': rel,
                              'line': loc(ts[i][1]), 'origin': origin,
                              'args': arguments(ts, match, i+1) if name in NATIVE | HOOK else []})
    return definitions, calls, constants, manifests


def cpp_inventory(repo):
    """Registration is evidence of binding, not proof of correct semantics."""
    natives, overrides, externals, constants = {}, {}, {}, {}
    review, methods, raw = [], {}, {}
    files = sorted((repo / 'common').rglob('*.cpp'))
    clean = {}
    for path in files:
        text = path.read_text(encoding='utf-8', errors='replace')
        raw[path] = text
        # Preserve string literals and line positions while removing comments.
        text = re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/',
                      lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]), text, flags=re.S)
        clean[path] = text
        for m in re.finditer(r'\b(?:constexpr|const)\s+[\w:]+\s+(\w+)\s*=\s*(0x[\da-fA-F]+|\d+)\s*;', text):
            constants[m[1]] = int(m[2], 0) if m[2].startswith('0x') else int(m[2])
        ts = tokens(text)
        match = pairs(ts)
        positions = {offset: i for i, (_, offset) in enumerate(ts)}
        for m in re.finditer(r'\b(\w+::\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:->[^{}]+)?\{', text):
            opening = positions.get(m.end()-1)
            if opening in match:
                end = ts[match[opening]][1]
                methods[m[1]] = (text[m.end():end], raw[path][m.end():end])
    for path, text in clean.items():
        rel = path.relative_to(repo).as_posix()
        cpp_ts = tokens(text)
        cpp_pairs = pairs(cpp_ts)
        openings = {offset: i for i, (t, offset) in enumerate(cpp_ts) if t == '{'}
        positions = {offset: i for i, (_, offset) in enumerate(cpp_ts)}
        for m in re.finditer(r'\b(register_(?:thiscall|cdecl|stdcall|fastcall|float_to_int)|winapi|override_function|safeOverrideFunction|safeOverride|safe|dummyfy|bindExternal|register_external)\s*\(\s*("[^"]+"|\w+)', text):
            api, key = m[1], m[2]
            where = f'{rel}:{text.count(chr(10), 0, m.start())+1}'
            paren = positions.get(text.find('(',m.start()))
            call_end = cpp_ts[cpp_pairs[paren]][1] if paren in cpp_pairs else m.end()
            body_start = text.find('{',m.end(),call_end)
            bi = openings.get(body_start)
            body, original_body = '', ''
            if bi in cpp_pairs:
                end = cpp_ts[cpp_pairs[bi]][1]
                body = text[body_start+1:end]
                original_body = raw[path][body_start+1:end]
            else:
                if api == 'register_float_to_int':
                    body, original_body = methods.get('Cpu32::register_float_to_int', ('', ''))
                member = re.search(r'&\s*(\w+::\w+)', text[m.end():call_end])
                if member:
                    body, original_body = methods.get(member[1], ('', ''))
            findings = []
            if bi in cpp_pairs or original_body:
                if not body.strip():
                    findings.append('empty_body')
                elif re.fullmatch(r'\s*return\s+(?:-?\d+|true|false|nullptr|\{\}|""|std::string\(""\))\s*;\s*', body):
                    findings.append('constant_return_review')
                if re.search(r'TODO|FIXME|not implemented|unimplemented|placeholder|dummy|suppress', original_body, re.I):
                    findings.append('incomplete_marker_review')
            if api == 'dummyfy':
                findings.append('dummy_wrapper')
            if findings:
                review.append({'api': api, 'name': key.strip('"').upper(), 'location': where,
                               'findings': ';'.join(findings),
                               'body_excerpt': re.sub(r'\s+', ' ', original_body).strip()[:240]})
            if api == 'winapi' or api.startswith('register_') and api != 'register_external':
                addr = number(key, {k.upper(): v for k, v in constants.items()})
                if addr is None:
                    continue
                # Read a balanced lambda body, rather than a line containing a
                # zero return in one of its validation branches.
                known_body = bi in cpp_pairs or bool(original_body)
                trivial = known_body and bool(re.fullmatch(r'\s*(?:return\s+(?:0|1|false|true)\s*;)?\s*', body))
                status = 'registered_trivial' if trivial else 'registered_review_semantics' if known_body else 'registered_indirect_review_semantics'
                natives[addr] = {'location': where, 'status': status}
            elif key.startswith('"'):
                (externals if api in ('bindExternal', 'register_external') else overrides)[key[1:-1].upper()] = where
    # An alias is included only for the actual wrappers above; arbitrary string
    # literals elsewhere in the code are not registrations.
    return natives, overrides, externals, review


def dat_ir(path):
    dat = Dat(str(path))
    by_address = collections.defaultdict(list)
    for i, sym in enumerate(dat.symbols):
        if sym['type'] in (5, 6, 7) and isinstance(sym['value'], int) and not sym['flags'] & (4 | 8) and (sym['type'] != 5 or sym['flags'] & 1):
            by_address[sym['value']].append(i)
    starts = sorted(by_address)
    calls, unresolved = [], []
    recent = []
    si = -1
    pc = 0
    while pc < len(dat.code):
        while si + 1 < len(starts) and starts[si+1] <= pc:
            si += 1
            recent = []
        op = dat.code[pc]
        if op not in OPS:
            raise ValueError(f'Unknown DAT opcode {op} at {pc:#x}; cannot count calls reliably')
        size = 6 if op == 245 else 5 if op in IMM else 1
        if pc + size > len(dat.code):
            raise ValueError(f'Truncated DAT instruction at {pc:#x}')
        operand = struct.unpack_from('<I', dat.code, pc+1)[0] if size >= 5 else None
        if op in (61, 62):
            targets = by_address.get(operand, []) if op == 61 else [operand]
            callee = dat.sym(targets[0]) if targets else f'<ADDRESS:{operand:#x}>'
            callers = [dat.sym(i) for i in by_address[starts[si]]] if si >= 0 else ['<GLOBAL>']
            call = {'caller': callers[0], 'caller_aliases': callers[1:], 'callee': callee.upper(), 'pc': hex(pc),
                    'kind': 'external' if op == 62 else 'script', 'origin': 'dat'}
            # Keep callback references separate from BL/BE counts. CoM queues
            # important story callbacks through AI_Function and LeGo FF; a
            # zero direct-call count does not make those functions unused.
            wrapper = call['callee']
            callback_arg = 1 if re.fullmatch(r'AI_FUNCTION(?:_[INS]{1,2})?', wrapper) else \
                           0 if re.fullmatch(r'FF_APPLY[A-Z]*|CALLAFTERWORLDLOADED|MEM_CALL(?:BYID|BYSTRING)?', wrapper) else None
            if callback_arg is not None and targets:
                argc = dat.symbols[targets[0]]['count']
                call['callback_argument'] = callback_arg
                if callback_arg < argc <= len(recent) and all(o in (64, 65, 67, 245) for o, _ in recent[-argc:]):
                    arg_op, arg_value = recent[-argc+callback_arg]
                    symbol = None
                    if arg_op == 64:
                        symbol = dat.symbols[arg_value] if arg_value < len(dat.symbols) else None
                    elif arg_op == 65 and arg_value < len(dat.symbols):
                        pushed = dat.symbols[arg_value]
                        if pushed['flags'] & 1 and pushed['type'] == 5:
                            symbol = pushed
                        elif pushed['flags'] & 1 and pushed['type'] == 2 and pushed['value']:
                            index = pushed['value'][0]
                            symbol = dat.symbols[index] if 0 <= index < len(dat.symbols) else None
                        elif wrapper == 'MEM_CALLBYSTRING' and pushed['flags'] & 1 and pushed['type'] == 3 and pushed['value']:
                            name = pushed['value'][0].upper()
                            symbol = next((s for s in dat.symbols if s['name'].upper() == name), None)
                    if symbol is not None and symbol['type'] == 5 and symbol['flags'] & 1:
                        call['callback_target'] = symbol['name'].upper()
            if call['callee'] in NATIVE | HOOK:
                # Only the immediately preceding operand is unambiguous without
                # dataflow analysis. Everything else remains explicitly dynamic.
                value = None
                previous = recent[-1] if recent else None
                if previous and previous[0] == 64:
                    value = previous[1]
                elif previous and previous[0] == 65 and previous[1] < len(dat.symbols):
                    s = dat.symbols[previous[1]]
                    if s['flags'] & 1 and s['type'] == 2 and s['value']:
                        value = s['value'][0]
                if call['callee'] in NATIVE:
                    call['address'] = value if value else None
                else:
                    # Hooks' last operand is the callback, not the address.
                    call['callback_operand'] = value
                    # Normal HookEngine[FI](address, length, callback) consists
                    # of three consecutive pushes. More complex sites stay dynamic.
                    if len(recent) >= 3 and all(o in (64, 65) for o, _ in recent[-3:]):
                        adr_op, adr_arg = recent[-3]
                        if adr_op == 64:
                            call['address'] = adr_arg
                        elif adr_arg < len(dat.symbols):
                            s = dat.symbols[adr_arg]
                            if s['flags'] & 1 and s['type'] == 2 and s['value']:
                                call['address'] = s['value'][0]
                    if value is not None and value < len(dat.symbols):
                        call['callback'] = dat.sym(value)
            calls.append(call)
            if not targets:
                unresolved.append(call)
        recent.append((op, operand))
        recent = recent[-8:]
        pc += size
    return dat, calls, unresolved


def write_csv(path, rows, fields):
    with path.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('ikarus', 'lego', 'mod'):
        ap.add_argument('--' + name, type=Path)
    ap.add_argument('--dat', type=Path, help='Authoritative static bytecode call counts')
    ap.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument('--out', type=Path, default=Path('build/ikarus-audit'))
    ap.add_argument('--disassemble', action='store_true', help='Also write the complete DAT disassembly')
    ap.add_argument('--top', type=int, default=30)
    args = ap.parse_args()
    if not args.mod and not args.dat:
        ap.error('Supply --mod and/or --dat')
    roots = [(n, getattr(args, n)) for n in ('ikarus', 'lego', 'mod') if getattr(args, n)]
    print('Reading sources...', flush=True)
    definitions, source_calls, constants, manifests = source_ir(roots)
    print('Reading C++ registrations...', flush=True)
    natives, overrides, externals, review = cpp_inventory(args.repo.resolve())
    hook_names = {}
    header = args.repo / 'common/game/compatibility/hookaddresses.h'
    if header.exists():
        hook_names = {int(m[2], 16): m[1] for m in re.finditer(r'constexpr\s+uint32_t\s+(\w+)\s*=\s*(0x[\da-fA-F]+)', header.read_text())}
    hook_cpp = (args.repo / 'common/game/compatibility/directmemory_hooks.cpp').read_text(encoding='utf-8')
    answered_body = hook_cpp.split('bool DirectMemory::isHookAnswered', 1)[-1].split('return answered', 1)[0]
    declared_hooks = {addr for addr, name in hook_names.items() if 'HookAdr::' + name in answered_body}
    dat, byte_calls, unresolved = dat_ir(args.dat) if args.dat else (None, [], [])
    # DAT constants rescue addresses that the approximate decompiler emitted as
    # uninitialised local variables. Qualified locals must not leak into globals.
    if dat:
        for sym in dat.symbols:
            if sym['type'] == 2 and sym['flags'] & 1 and sym['value']:
                constants[sym['name'].upper()] = sym['value'][0]
    inventory = []
    for call in source_calls:
        if call['callee'] not in NATIVE | HOOK:
            continue
        argv = call['args']
        expr = argv[0] if call['callee'] in HOOK and argv else argv[-1] if argv else ''
        # Prefer function-scoped constants from DAT over ambiguous short names.
        scoped = dict()
        for token in re.findall(r'[A-Za-z_]\w*', expr):
            key = call['caller'] + '.' + token.upper()
            if key in constants:
                scoped[token.upper()] = constants[key]
        addr = number(expr, collections.ChainMap(scoped, constants))
        if addr == 0:
            addr = None
        status = 'dynamic_expression'
        handler = ''
        if call['caller'] in overrides:
            status, handler = 'caller_overridden_review_semantics', overrides[call['caller']]
        elif addr is not None and call['callee'] in NATIVE:
            info = natives.get(addr)
            status = info['status'] if info else 'missing_native'
            handler = info['location'] if info else ''
        elif addr is not None:
            status = 'hook_declared_review_semantics' if addr in declared_hooks else 'missing_hook_bridge'
        if call['callee'] in HOOK and len(argv)>=3 and argv[2].upper() in ('_FF_HOOK', '_AI_FUNCTION_EVENT'):
            status = 'special_event_adapter_review_semantics'
        inventory.append({**call, 'address': hex(addr & 0xffffffff) if addr is not None else '',
                          'callback': argv[2] if call['callee'] in HOOK and len(argv) >= 3 else '',
                          'expression': expr, 'status': status, 'handler': handler})
    for call in byte_calls:
        if call['callee'] not in NATIVE | HOOK:
            continue
        addr = call.get('address')
        info = natives.get(addr)
        status = info['status'] if info else 'missing_native' if addr is not None else 'dynamic_expression'
        if call['callee'] in HOOK and addr is not None:
            status = 'hook_declared_review_semantics' if addr in declared_hooks else 'missing_hook_bridge'
        if call.get('callback', '').upper() in ('_FF_HOOK', '_AI_FUNCTION_EVENT'):
            status = 'special_event_adapter_review_semantics'
        handler = info['location'] if info else ''
        if call['caller'] in overrides:
            status, handler = 'caller_overridden_review_semantics', overrides[call['caller']]
        inventory.append({**call, 'address': hex(addr & 0xffffffff) if addr is not None else '',
                          'status': status, 'handler': handler})
    primary_calls = byte_calls if dat else [c for c in source_calls if c['origin'] == 'mod']
    counts = collections.Counter(c['callee'] for c in primary_calls)
    if dat:
        for sym in dat.symbols:
            if sym['type'] == 5 and sym['flags'] & 1 and not sym['flags'] & 4:
                counts.setdefault(sym['name'].upper(), 0)
    else:
        for definition in definitions:
            if definition['kind'] == 'func':
                counts.setdefault(definition['name'], 0)
    callers = collections.defaultdict(set)
    for call in primary_calls:
        callers[call['callee']].add(call['caller'])
    defined = {d['name'] for d in definitions if d['body']}
    dat_externals = {s['name'].upper() for s in dat.symbols if s['type'] == 5 and s['flags'] & 8} if dat else set()
    rows = []
    for name, count in counts.most_common():
        status = 'external_bound' if name in externals else 'overridden_review_semantics' if name in overrides else 'script_defined'
        if name in dat_externals and name not in externals | overrides:
            status = 'missing_external'
        elif not dat and name not in defined | externals.keys() | overrides.keys():
            status = 'unresolved_source_symbol'
        rows.append({'function': name, 'static_call_sites': count, 'distinct_callers': len(callers[name]),
                     'status': status, 'binding': externals.get(name, overrides.get(name, ''))})
    callback_rows = []
    for call in byte_calls:
        if 'callback_argument' not in call:
            continue
        target = call.get('callback_target', '')
        callback_rows.append({'caller': call['caller'], 'wrapper': call['callee'], 'pc': call['pc'],
                              'target': target,
                              'status': 'dynamic_callback' if not target else 'overridden_review_semantics' if target in overrides
                                        else 'external_bound' if target in externals else 'missing_external' if target in dat_externals
                                        else 'script_defined_review_semantics',
                              'binding': overrides.get(target, externals.get(target, ''))})
    summary = {'schema': 1, 'count_kind': 'static_call_sites', 'primary': 'dat' if dat else 'mod_source',
               'source_files': len(manifests), 'source_definitions': len(definitions),
               'source_call_sites': len(source_calls), 'dat_symbols': len(dat.symbols) if dat else 0,
               'dat_call_sites': len(byte_calls), 'unresolved_bytecode_calls': len(unresolved),
               'callback_sites': len(callback_rows),
               'resolved_callback_sites': sum(bool(r['target']) for r in callback_rows),
               'function_status': dict(collections.Counter(r['status'] for r in rows)),
               'called_function_status': dict(collections.Counter(r['status'] for r in rows if r['static_call_sites'])),
               'native_status': dict(collections.Counter(r['status'] for r in inventory)),
               'native_status_by_origin': {origin: dict(collections.Counter(r['status'] for r in inventory if r['origin'] == origin))
                                           for origin in sorted({r['origin'] for r in inventory})},
               'limitations': ['Counts do not measure runtime frequency or campaign playability.',
                               'Registrations and overrides need semantic review; partial adapters may exist.',
                               'Callback references are reported separately; only unambiguous consecutive pushes are resolved.',
                               'Computed callback targets and native dynamic addresses remain unresolved.',
                               'Source hooks need an event and register/stack contract review.',
                               'Library source versions may differ from libraries embedded in DAT.']}
    if args.dat:
        summary['dat_sha256'] = hashlib.sha256(args.dat.read_bytes()).hexdigest()
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    # JSONL is a simple, streamable IR, with source arguments for engine calls.
    with (args.out / 'calls.jsonl').open('w', encoding='utf-8') as stream:
        for call in source_calls + byte_calls:
            stream.write(json.dumps(call, ensure_ascii=True) + '\n')
    with (args.out / 'definitions.jsonl').open('w', encoding='utf-8') as stream:
        for definition in definitions:
            stream.write(json.dumps(definition) + '\n')
    (args.out / 'sources.json').write_text(json.dumps(manifests, indent=2), encoding='utf-8')
    write_csv(args.out / 'functions.csv', rows, ['function', 'static_call_sites', 'distinct_callers', 'status', 'binding'])
    write_csv(args.out / 'callback-references.csv', callback_rows, ['caller', 'wrapper', 'pc', 'target', 'status', 'binding'])
    for item in review:
        item['static_call_sites'] = counts[item['name']]
    write_csv(args.out / 'implementation-review.csv', review,
              ['api', 'name', 'static_call_sites', 'location', 'findings', 'body_excerpt'])
    markers = []
    for path in sorted((args.repo / 'common').rglob('*.cpp')):
        for line, code in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            if re.search(r'TODO|FIXME|not implemented|unimplemented|placeholder|suppress', code, re.I):
                markers.append({'file': path.relative_to(args.repo).as_posix(), 'line': line, 'excerpt': code.strip()[:240]})
    write_csv(args.out / 'markers.csv', markers, ['file', 'line', 'excerpt'])
    write_csv(args.out / 'native-and-hooks.csv', inventory,
              ['origin', 'caller', 'callee', 'callback', 'file', 'line', 'pc', 'address', 'expression', 'status', 'handler'])
    missing = [r for r in rows if r['static_call_sites'] and r['status'] in ('missing_external', 'unresolved_source_symbol')]
    write_csv(args.out / 'missing-functions.csv', missing, ['function', 'static_call_sites', 'distinct_callers', 'status', 'binding'])
    edges = collections.Counter((c['caller'], c['callee']) for c in primary_calls)
    write_csv(args.out / 'callgraph.csv',
              ({'caller': k[0], 'callee': k[1], 'static_call_sites': n} for k, n in sorted(edges.items())),
              ['caller', 'callee', 'static_call_sites'])
    # Keep DAT/source inventories separate: adding both would count the same
    # compiled site twice, and current library checkouts can differ from DAT.
    address_rows = collections.defaultdict(list)
    for row in inventory:
        if row['origin'] == ('dat' if dat else 'mod') and row['address']:
            address_rows[(row['address'], row['status'])].append(row)
    write_csv(args.out / 'addresses.csv',
              ({'address': addr, 'status': status, 'static_call_sites': len(sites),
                'wrapper_call_sites': sum(counts[c] for c in {s['caller'] for s in sites}),
                'callers': ';'.join(sorted({s['caller'] for s in sites})),
                'callbacks': ';'.join(sorted({s.get('callback', '') for s in sites})),
                'handler': sites[0]['handler']} for (addr, status), sites in sorted(address_rows.items())),
              ['address', 'status', 'static_call_sites', 'wrapper_call_sites', 'callers', 'callbacks', 'handler'])
    if args.disassemble:
        if not dat:
            ap.error('--disassemble requires --dat')
        dat.write_disassembly(args.out / 'GOTHIC.disasm.txt')
    print(json.dumps(summary, indent=2))
    print('\nMost called functions (static sites):')
    for row in rows[:args.top]:
        print(f"{row['static_call_sites']:7} {row['function']:42} {row['status']}")
    print('\nMissing functions:')
    for row in missing:
        print(f"{row['static_call_sites']:7} {row['function']}")
    print(f'\nFull IR and reports: {args.out.resolve()}')


if __name__ == '__main__':
    main()
