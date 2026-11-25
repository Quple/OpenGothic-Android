#!/usr/bin/env python3
"""Tasks for the compatibility layer of Ikarus and LeGo, one file per gap, written so that a person or an AI agent can
implement one without searching: what the mod calls, the C++ signature from the engine database, who calls it in the
scripts (with the bytecode), what the game saw at run time, a skeleton of the handler and how to check it.

    python tools/ikarus/worklist.py --audit build-ikarus/com/audit --dat build-ikarus/com/GOTHIC.DAT \
        [--gaps log.txt ikarus-gaps.json ...] [--exe <Gothic2.exe>] [--engine G2A] --out build-ikarus/com/worklist

Inputs: the output of analyze.py (--audit), the DAT (callers' bytecode), the engine database (enginedb.py build), the
records of the game (lines "IKGAP {...}" of log.txt or ikarus-gaps.json, see common/game/compatibility/gaplog.h) and,
optionally, the original executable for the x86 code around a hook inside a function (needs capstone:
pip install --target build-ikarus/pylib capstone).

Outputs: INDEX.md (the tasks by priority), tasks/NNN-*.md, tasks.json. Nothing of the mod is executed.
"""
import argparse
import collections
import csv
import json
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
PYLIB = REPO / 'build-ikarus' / 'pylib'
if PYLIB.is_dir():
    sys.path.insert(0, str(PYLIB))

from enginedb import EngineDb, DEFAULT_OUT   # noqa: E402

GAP_STATUSES = ('missing_native', 'missing_hook_bridge', 'registered_trivial')
REVIEW_STATUSES = ('hook_declared_review_semantics', 'registered_review_semantics', 'registered_indirect_review_semantics')
ENTRY = re.compile(r'^(INIT_GLOBAL|STARTUP_GLOBAL|INIT_\w+|STARTUP_\w+|DIA_\w+|ZS_\w+|B_\w+|SPELL_\w+|USE_\w+|EVT_\w+)$')
LIB_PREFIX = re.compile(r'^(MEM_|MEMINT_|CALL_|CALLINT_|ASM_|ASMINT_|HOOK|_HOOK|LEGO_|FF_|_FF_|PM_|_PM_|HT_|_HT_|LIST_|'
                        r'STR_|PRINT_|VIEW_|BAR_|BUTTON_|CURSOR_|TIMER|AI_FUNCTION|_AI_|TRIA_|BUFF_|ANIM8|EVENTPTR|'
                        r'SB_|MEMINT|LOCALS|_@|_\^|MKF|ADDF|SUBF|MULF|DIVF|TRUNCF|ROUNDF|CASTTOINT|CASTFROMINT|PERMMEM|'
                        r'CREATE|DELETE|FREE|NEW|HLP_|WRITENOP|REPLACEENGINEFUNC|ISHOOKED|REMOVEHOOK)')

RULES = """\
Rules of this layer (docs/IKARUS.md, the handoffs in docs/):
- Calls of the engine are answered in C++ by `cpu.register_thiscall/stdcall/cdecl(address, lambda)` in
  `common/game/compatibility/directmemory*.cpp`; the arguments are popped in the order of the declaration, `self` first.
- Hooks (`HookEngineF`) are events: the address goes to `hookaddresses.h` and to `DirectMemory::isHookAnswered`, and
  the place in OpenGothic that does the same thing calls `fireHook(address, regs)` with the registers and the stack
  that the script reads (ECX, ESP+4, ...), then takes back what the script changed.
- A whole function of the libraries can be replaced by its name (`vm.override_function`, `safeOverrideFunction`):
  that is right when the function only exists to poke the memory of the engine.
- No empty handler, no handler that only returns 0, no hidden log line, no address marked as answered without the
  effect. When the effect cannot be done, leave it in the list and say why in the task.
- Pointers of characters and items are handles (`InstanceHandles`, `handles.instanceAt(ptr)`), the vobs that
  scripts find by name are movers (`MoverPool`); other memory of the engine does not exist.
"""


def read_csv(path):
    if not path.exists():
        return []
    with path.open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))


def read_gaps(paths):
    """The records of the game: ikarus-gaps.json (with counts) or the IKGAP lines of log.txt (the first time of each)"""
    out = {}
    for p in paths:
        p = Path(p)
        if not p.exists():
            print(f'warning: {p} does not exist', file=sys.stderr)
            continue
        text = p.read_text(encoding='utf-8', errors='replace')
        records = []
        if p.suffix.lower() == '.json':
            try:
                data = json.loads(text)
            except json.JSONDecodeError:
                data = {}
            g = data.get('gaps', [])
            records = g.get('gaps', []) if isinstance(g, dict) else g      # autorun-report.json / ikarus-gaps.json
            # the errors of the log of an autorun that concern the scripts: "Accessing member ... (in FUNC)", the layer
            for e in data.get('errors', []):
                m = re.search(r'\(in (\w+)\)\s*$', e)
                if not m and not re.search(r'mem_|DMA|ASMINT|Ikarus|LeGo|not implemented|translation', e, re.I):
                    continue
                records.append({'kind': 'log', 'name': re.sub(r'\d+', 'N', e)[:160],
                                'stack': [m[1]] if m else [], 'count': 1})
            # the checks of the autorun (comcheck test ...) that failed
            for c in data.get('console', []):
                if c.get('text', '').startswith('FAIL'):
                    records.append({'kind': 'check', 'name': c['text'][:200], 'count': 1})
        else:
            for m in re.finditer(r'IKGAP (\{.*\})\s*$', text, re.M):
                try:
                    records.append(json.loads(m[1]))
                except json.JSONDecodeError:
                    pass
        for r in records:
            key = (r.get('kind'), r.get('address'), r.get('name', ''), r.get('class', ''))
            prev = out.get(key)
            if prev and r.get('kind') == 'log':
                prev['count'] += r.get('count', 1)
            elif prev:
                prev['count'] = max(prev.get('count', 1), r.get('count', 1))
            else:
                out[key] = dict(r, count=r.get('count', 1))
    return list(out.values())


def parse_int(v):
    if v is None or v == '':
        return None
    if isinstance(v, int):
        return v
    v = str(v).strip()
    return int(v, 16) if v.lower().startswith('0x') else int(v)


class Graph:
    def __init__(self, rows):
        self.callers = collections.defaultdict(set)
        for r in rows:
            self.callers[r['callee']].add(r['caller'])

    def reach(self, start, limit=4000):
        """The functions from which `start` can be reached (its transitive callers)"""
        seen, todo = set(), list(start)
        while todo and len(seen) < limit:
            f = todo.pop()
            for c in self.callers.get(f, ()):
                if c not in seen:
                    seen.add(c)
                    todo.append(c)
        return seen


def x86_disasm(exe, va, before=24, after=40):
    """The x86 code around an address of the original executable, None without capstone or the file"""
    try:
        import capstone
    except ImportError:
        return None
    data = exe.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    sec = pe + 24 + opt
    rva = va - base
    for i in range(nsec):
        vsize, vaddr, raw_size, raw = struct.unpack_from('<IIII', data, sec + 40*i + 8)
        if vaddr <= rva < vaddr + max(vsize, raw_size):
            off = raw + rva - vaddr
            md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
            # start a bit before, at an instruction boundary found by decoding forward from further back
            lines = []
            for back in range(before, -1, -1):
                code = data[off-back:off+after]
                ins = list(md.disasm(code, va-back))
                if any(i.address == va for i in ins):
                    for i in ins:
                        mark = '=>' if i.address == va else '  '
                        lines.append(f'{mark} {i.address:08x}  {i.mnemonic:6} {i.op_str}')
                    return lines
            return [f'   (no instruction starts at {va:#x})']
    return None


def slug(text):
    return re.sub(r'[^A-Za-z0-9]+', '-', text).strip('-')[:60] or 'task'


def annotate(line, db):
    """A number of the bytecode that is an address of the engine gets its name"""
    def repl(m):
        text = m[0]
        v = int(text, 16) if text.lower().startswith('0x') else int(text)
        if not 0x400000 <= v < 0x1000000:
            return text
        name = db.describe(v)
        return f'{text} <{name}>' if name else text
    return re.sub(r'(?<=pushi    )\d{7,8}\b|(?<=\(= )0x[0-9a-f]{6,8}(?=\))', repl, line)


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--audit', type=Path, required=True, help='the output directory of analyze.py')
    ap.add_argument('--dat', type=Path, help='GOTHIC.DAT of the mod (the bytecode of the callers)')
    ap.add_argument('--gaps', type=Path, nargs='*', default=[], help='log.txt / ikarus-gaps.json of runs of the game')
    ap.add_argument('--exe', type=Path, help='the original Gothic2.exe / Gothic.exe (x86 code of hooks inside functions)')
    ap.add_argument('--engine', default='G2A')
    ap.add_argument('--enginedb', type=Path, default=DEFAULT_OUT)
    ap.add_argument('--review', action='store_true', help='also the registered addresses that need a review of the semantics')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()

    db = EngineDb.load(args.engine, args.enginedb)
    dat = None
    if args.dat:
        from disasm import Dat
        dat = Dat(str(args.dat))
    addresses = read_csv(args.audit / 'addresses.csv')
    sites = read_csv(args.audit / 'native-and-hooks.csv')
    graph = Graph(read_csv(args.audit / 'callgraph.csv'))
    functions = {r['function']: r for r in read_csv(args.audit / 'functions.csv')}
    missing = read_csv(args.audit / 'missing-functions.csv')
    gaps = read_gaps(args.gaps)

    # the classes that already have handlers, and where: an example for a new one
    handled_by_class = collections.defaultdict(set)
    for r in addresses:
        if r['handler'] and r['status'].startswith('registered'):
            f, _ = db.function_at(parse_int(r['address']))
            if f and f.get('class'):
                handled_by_class[f['class']].add(r['handler'].split(':')[0])

    tasks = {}

    def task(key, **kw):
        t = tasks.setdefault(key, {'key': key, 'runtime': [], 'sites': [], 'callers': set(), 'callbacks': set(),
                                   'statuses': set(), 'static_call_sites': 0, 'wrapper_call_sites': 0})
        for k, v in kw.items():
            if isinstance(t.get(k), set):
                t[k] |= set(v)
            elif k in ('static_call_sites', 'wrapper_call_sites'):
                t[k] += int(v or 0)
            else:
                t.setdefault(k, v)
        return t

    wanted = GAP_STATUSES + (REVIEW_STATUSES if args.review else ())
    for r in addresses:
        if r['status'] not in wanted:
            continue
        addr = parse_int(r['address'])
        kind = 'hook' if 'hook' in r['status'] else 'call'
        task((kind, addr), kind=kind, address=addr, statuses=[r['status']],
             callers=[c for c in r['callers'].split(';') if c], callbacks=[c for c in r['callbacks'].split(';') if c],
             static_call_sites=r['static_call_sites'], wrapper_call_sites=r['wrapper_call_sites'], handler=r['handler'])
    for s in sites:
        if s['address'] and s['status'] in wanted:
            k = ('hook' if 'hook' in s['status'] else 'call', parse_int(s['address']))
            if k in tasks:
                tasks[k]['sites'].append(s)
    for r in missing:
        task(('external', r['function']), kind='external', name=r['function'], statuses=[r['status']],
             static_call_sites=r['static_call_sites'])
    for g in gaps:
        kind = g.get('kind')
        if kind in ('call', 'hook', 'unmapped'):
            key = (kind, parse_int(g.get('address')))
        elif kind == 'field':
            key = ('field', g.get('class', ''), parse_int(g.get('address')))
        else:
            key = (kind, g.get('name', '') or g.get('detail', '')[:80])
        t = task(key, kind=kind, address=parse_int(g.get('address')), name=g.get('name'), cls=g.get('class'),
                 statuses=['runtime'])
        t['runtime'].append(g)

    # priority: seen in the game, then how much of the scripts reaches it
    for t in tasks.values():
        start = set(t['callers'])
        for g in t['runtime']:
            start |= set(g.get('stack', []))
        reach = graph.reach(start | ({t['name']} if t.get('name') else set()))
        reach |= start
        t['reach'] = len(reach)
        t['entries'] = sorted(f for f in reach if ENTRY.match(f))[:12]
        t['story'] = sorted(f for f in reach if f.startswith(('DIA_', 'B_', 'ZS_', 'INIT_', 'STARTUP_')))[:12]
        hits = sum(g.get('count', 1) for g in t['runtime'])
        t['hits'] = hits
        t['priority'] = (3 if t['kind'] == 'crash' else 2 if t['kind'] == 'check' else 1 if hits else 0, hits,
                         len(t['story']) > 0, t['reach'], t['static_call_sites'])

    ordered = sorted(tasks.values(), key=lambda t: t['priority'], reverse=True)
    out = args.out
    (out / 'tasks').mkdir(parents=True, exist_ok=True)
    for old in (out / 'tasks').glob('*.md'):
        old.unlink()
    index = ['# Ikarus/LeGo work list', '',
             f'Engine {db.engine}; audit `{args.audit}`; runtime records: {len(gaps)} from {len(args.gaps)} file(s).', '',
             'Priority: seen in the game first (hits), then reached from dialogs/routines/init, then the number of',
             'script functions that can reach it. A task is closed when the audit no longer lists it and a run',
             'of the game (see docs/IKARUS_AUTOMATION.md) has no record of it.', '',
             '| # | kind | what | status | hits | reach | file |', '|---|---|---|---|---|---|---|']
    machine = []
    for n, t in enumerate(ordered, 1):
        title, body = render(t, db, dat, args, handled_by_class, functions)
        name = f'{n:03d}-{slug(title)}.md'
        (out / 'tasks' / name).write_text(body, encoding='utf-8')
        index.append(f"| {n} | {t['kind']} | {title.replace('|', '/')} | {', '.join(sorted(t['statuses']))} | "
                     f"{t['hits']} | {t['reach']} | [tasks/{name}](tasks/{name}) |")
        machine.append({'n': n, 'file': f'tasks/{name}', 'title': title, 'kind': t['kind'],
                        'address': f"0x{t['address']:08x}" if isinstance(t.get('address'), int) else None,
                        'statuses': sorted(t['statuses']), 'hits': t['hits'], 'reach': t['reach'],
                        'callers': sorted(t['callers']), 'entries': t['entries']})
    (out / 'INDEX.md').write_text('\n'.join(index) + '\n', encoding='utf-8')
    (out / 'tasks.json').write_text(json.dumps(machine, indent=1), encoding='utf-8')
    print(f'{len(ordered)} tasks -> {out.resolve() / "INDEX.md"}')
    for m in machine[:15]:
        print(f"{m['n']:4} {m['kind']:9} {m['title'][:90]}  hits={m['hits']} reach={m['reach']}")


def render(t, db, dat, args, handled_by_class, functions):
    kind = t['kind']
    addr = t.get('address')
    lines = []
    f, off = (db.function_at(addr) if isinstance(addr, int) else (None, 0))
    if kind in ('call', 'hook'):
        what = (f"{f['name']}+0x{off:x}" if off else f['name']) if f else f'unknown 0x{addr:08x}'
        title = f"{'hook' if kind == 'hook' else 'call'} {what} (0x{addr:08x})"
    elif kind == 'field':
        hits = db.field(t.get('cls') or '', addr or 0) if t.get('cls') else []
        fname = '/'.join(h['name'] for h in hits) or '?'
        title = f"field {t.get('cls')}+0x{(addr or 0):x} {fname}"
    elif kind == 'unmapped':
        title = f"memory 0x{(addr or 0):08x} {db.describe(addr or 0) or ''}".strip()
    elif kind == 'external':
        title = f"external {t.get('name')}"
    else:
        title = f"{kind} {t.get('name') or ''}".strip()
    lines += [f'# {title}', '']
    lines.append(f"Status: {', '.join(sorted(t['statuses']))}. Engine {db.engine}. Static call sites "
                 f"{t['static_call_sites']}, sites of the wrappers {t['wrapper_call_sites']}, script functions that "
                 f"reach it {t['reach']}, records of the game {t['hits']}.")
    if t.get('handler'):
        lines.append(f"Current handler: `{t['handler']}`.")
    lines.append('')

    if f:
        lines += ['## The engine', '', '```', f"0x{f['address']:08x}  {f['sig']}", f"calling convention: {f['conv']}"
                  + (', virtual' if f.get('virtual') else '') + (f", the hook is 0x{off:x} bytes inside" if off else ''),
                  '```', '']
        cls = f.get('class')
        if cls and db.cls(cls):
            lay = db.layout(cls)
            c = db.cls(cls)
            vt = db.vtable(cls)
            lines.append(f"`{cls}`: sizeof {('0x%x' % c['size']) if c.get('size') else '?'}, bases "
                         f"{', '.join(c['bases']) or '-'}, vtable {('0x%08x' % vt) if vt else '-'}, "
                         f"{len(lay)} fields (`python tools/ikarus/enginedb.py class {cls}`).")
            lines.append('')
        if cls and handled_by_class.get(cls):
            lines.append(f"Other methods of `{cls}` are answered in: " +
                         ', '.join(f'`{h}`' for h in sorted(handled_by_class[cls])) + ' (follow their style).')
            lines.append('')
        if off and args.exe:
            code = x86_disasm(args.exe, addr)
            if code:
                code = [re.sub(r'(?<=[\s\[])0x[0-9a-f]{6}\b', lambda m: (lambda n: f'{m[0]} <{n}>' if n else m[0])(
                        db.describe(int(m[0], 16))), c) for c in code]
                lines += ['### The x86 code at the hook (original executable)', '', '```'] + code + ['```', '',
                          'The script of the hook reads the registers at `=>` (EAX, ECX, ESI, EDI, EBP of LeGo) and the '
                          'stack (ESP+n); `fireHook` must give it the values that these instructions use.', '']
            elif code is None:
                lines += ['(x86 code not shown: capstone or the executable is missing)', '']

    if kind == 'field':
        hits = db.field(t.get('cls') or '', addr or 0) if t.get('cls') else []
        lines += ['## The field', '']
        for h in hits:
            lines.append(f"- `{h['owner']}.{h['name']}{h.get('array', '')}` at +0x{h['offset']:x}, {h['size']} bytes, "
                         f"`{h['type']}`" + (f", bit {h['bit']}:{h['bits']}" if 'bits' in h else ''))
        lines += ['', 'The scripts read or write this field of a character/item through its handle '
                  '(`MEM_ReadInt(ptr+offset)`); today the handle memory gives 0 there and drops the write. '
                  'Fill it in `DirectMemory::syncHandleObject` (read) and take writes back from it (write), or replace '
                  'the script function that touches it.', '']

    if t['callers'] or t['callbacks']:
        lines += ['## The scripts', '']
        if t['callers']:
            lines.append('Direct callers: ' + ', '.join(f'`{c}`' for c in sorted(t['callers'])[:20]))
        if t['callbacks']:
            lines.append('Hook functions of the mod: ' + ', '.join(f'`{c}`' for c in sorted(t['callbacks'])[:20]))
        if t['entries']:
            lines.append('Reached from: ' + ', '.join(f'`{c}`' for c in t['entries']))
        lines.append('')
        if dat:
            shown = 0
            for name in sorted(t['callbacks'] - {''}) + sorted(t['callers']):
                if shown >= 3:
                    break
                code = dat.disasm_lines(name, limit=60)
                if not code:
                    continue
                code = [annotate(c, db) for c in code]
                shown += 1
                calls = functions.get(name.upper(), {}).get('static_call_sites', '?')
                lines += [f'### `{name}` (bytecode, {calls} call sites)', '', '```'] + code + ['```', '']
    if t['runtime']:
        lines += ['## What the game recorded', '']
        for g in t['runtime'][:5]:
            lines.append('```json')
            lines.append(json.dumps(g, indent=1, ensure_ascii=False)[:2500])
            lines.append('```')
        lines.append('')
    if kind == 'unmapped' and isinstance(addr, int):
        d, doff = db.data_at(addr)
        f2, foff = db.function_at(addr)
        lines += ['## The memory', '']
        if d:
            lines.append(f"0x{addr:08x} is `{d.get('decl') or d['name']}`" + (f' + 0x{doff:x}' if doff else '') +
                         (f", type `{d['type']}`" if d.get('type') else '') + ' in the original engine.')
        elif f2:
            lines.append(f"0x{addr:08x} is the code of `{f2['sig']}` + 0x{foff:x} (a script reads or patches the code).")
        else:
            lines.append(f'0x{addr:08x} is not a known global of the engine: a pointer that the layer gave out and '
                         'freed, or a field of an object that is not mirrored (follow the stack).')
        stacks = [g.get('stack', []) for g in t['runtime']]
        mods_fn = [f for s in stacks for f in s if not LIB_PREFIX.match(f)]
        lines += ['', 'Either mirror it (an object at this address in `DirectMemory::setupEngineMemory`, filled from '
                  'OpenGothic) or replace the first function of the stack that only pokes the engine'
                  + (f" (here: `{mods_fn[0]}`)" if mods_fn else '') + ' by `safeOverrideFunction`.', '']
    if kind == 'crash':
        g = t['runtime'][0] if t['runtime'] else {}
        lines += ['## The crash', '', f"{t.get('name')}.", '',
                  'Reproduce it alone: the dialog or the function in the stack, with `-autorun "wait 30; comcheck dlg run '
                  '<DIALOG>; wait 10"` (or `comcheck dlg scene <FUNCTION>`), on a debug build under a debugger '
                  '(`cdb -g -o Gothic2Notr.exe ...` gives the C++ stack). The last lines of log.txt:', '',
                  '```', g.get('detail', '')[:6000], '```', '']
    if kind == 'check':
        lines += ['## The failed check', '',
                  f"`{t.get('name')}`", '',
                  'A test of the game itself (`mods/devtools/lib/selftests.js` for `comcheck test`, '
                  '`mods/devtools/lib/archoloschecks.js`, `fixcases.js`) failed in an unattended run. Read the test, '
                  'decide whether the layer or the test is wrong (the original engine is the reference: '
                  '`enginedb.py lookup/stub`, the x86 code with `--exe`), fix it and run the same command again.', '']
    if kind in ('call', 'hook') and isinstance(addr, int):
        lines += ['## Skeleton', '', '```cpp', db.stub(addr, hook=(kind == 'hook')), '```', '']
    if kind == 'external':
        lines += ['## Skeleton', '', 'An external of the scripts that OpenGothic does not bind: add it in '
                  '`GameScript` (`bindExternal`) or, for one of the libraries, `vm.override_function` in DirectMemory.', '']
    lines += ['## Done when', '',
              '- `python tools/ikarus/modcheck.py --game <game> --ini <mod>.ini --fresh --only audit` no longer lists it;',
              '- an unattended run (`modcheck.py ... --run "<commands>"`, docs/IKARUS_AUTOMATION.md) has no record of it',
              '  and the check of the feature in that run passes;',
              '- the commit says what the player sees and what is left.', '', RULES]
    return title, '\n'.join(lines)


if __name__ == '__main__':
    main()
