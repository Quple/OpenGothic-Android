#!/usr/bin/env python3
"""The engine database of ZenGin, converted from the Gothic API headers of the Union team.

Every address of Gothic.exe / Gothic2.exe with its C++ signature and calling convention, the layouts of the classes
(the fields with offsets), the global variables, static members and the tables of virtual functions. With it an
address that a mod calls (CALL__thiscall), hooks (HookEngineF) or a field that it reads (MEM_ReadInt(ptr+off)) has a
name, a signature and a C++ skeleton for DirectMemory, without a person looking it up.

    python tools/ikarus/enginedb.py build --fetch            # clone gothic-api to build-ikarus/, write the JSON databases
    python tools/ikarus/enginedb.py lookup 0x437d9f 7a63c0   # the function (and the offset inside it)
    python tools/ikarus/enginedb.py field oCNpc 0x9ac        # the field at an offset of a class (bases included)
    python tools/ikarus/enginedb.py class zCView             # the layout
    python tools/ikarus/enginedb.py stub 0x749cb0            # a C++ skeleton of the handler (register_thiscall ...)
    python tools/ikarus/enginedb.py symbolize log.txt        # the addresses in a text get their names

Source: https://github.com/gothicuef/gothic-api (BSD-style licence, keep its notice). The databases are generated into
build-ikarus/enginedb/ and are not committed. Only the Python standard library is needed.
"""
import argparse
import bisect
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_API = REPO / 'build-ikarus' / 'gothic-api'
DEFAULT_OUT = REPO / 'build-ikarus' / 'enginedb'
API_URL = 'https://github.com/gothicuef/gothic-api'
SCHEMA = 1

ENGINES = {
    # key: (directory of gothic-api, the game of OpenGothic)
    'G1':  'Gothic_I_Classic',
    'G1A': 'Gothic_I_Addon',
    'G2':  'Gothic_II_Classic',
    'G2A': 'Gothic_II_Addon',
}
ALIASES = {'G2NOTR': 'G2A', 'NOTR': 'G2A', 'GOTHIC2': 'G2A', 'GOTHIC1': 'G1', 'GOTHIC': 'G1'}

CONVS = ('__thiscall', '__stdcall', '__cdecl', '__fastcall')
INTEGRAL = {'int', 'long', 'short', 'char', 'bool', 'unsigned', 'signed', 'unsigned int', 'unsigned long',
            'unsigned short', 'unsigned char', 'signed char', 'int32_t', 'uint32_t', 'BOOL', 'DWORD', 'WORD',
            'BYTE', 'UINT', 'ULONG', 'LONG', 'size_t', 'wchar_t', 'zBOOL', 'zDWORD', 'zWORD', 'zBYTE', 'zUINT32',
            'zINT', 'zUINT', 'zLONG'}
TWO_SLOTS = {'double', '__int64', 'unsigned __int64', 'long double'}


# ----------------------------------------------------------------------------------------------------------------------
# parsing of the undecorated MSVC names (Names.txt)

def _split_depth0(text, sep=','):
    out, depth, cur = [], 0, []
    for ch in text:
        if ch in '(<[':
            depth += 1
        elif ch in ')>]':
            depth -= 1
        if ch == sep and depth == 0:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if ''.join(cur).strip():
        out.append(''.join(cur).strip())
    return out


def _find_depth0(text, word):
    """The position of a word outside of (), <> and []"""
    depth = 0
    i = 0
    while i < len(text):
        ch = text[i]
        if ch in '(<[':
            depth += 1
        elif ch in ')>]':
            depth -= 1
        elif depth == 0 and text.startswith(word, i) and (i == 0 or not text[i-1].isalnum()) \
                and (i+len(word) >= len(text) or not (text[i+len(word)].isalnum() or text[i+len(word)] == '_')):
            return i
        i += 1
    return -1


def clean_type(t):
    t = re.sub(r'\b(?:class|struct|union|enum)\s+', '', t.strip())
    t = re.sub(r'\s+', ' ', t)
    t = re.sub(r'\s*([*&])', r'\1', t)
    t = t.replace('const*', 'const *').replace('const&', 'const &')
    return re.sub(r'\s*([*&])', r'\1', t).strip()


def kind_of_type(raw):
    """How a value of the type travels on the stack of x86: int, float, ptr, string (zSTRING const &), struct, wide"""
    t = raw.strip()
    is_enum = bool(re.match(r'\s*enum\b', t))
    c = clean_type(t)
    if c in ('void', ''):
        return 'void'
    if c.endswith(('*', '&')):
        base = c.rstrip('*&').strip()
        if re.fullmatch(r'zSTRING const|const zSTRING', base) and c.count('*') + c.count('&') == 1:
            return 'string'
        if re.fullmatch(r'char const|const char', base) and c.endswith('*'):
            return 'cstring'
        return 'ptr'
    if c in ('float',):
        return 'float'
    if c in TWO_SLOTS:
        return 'wide'
    if is_enum or c in INTEGRAL or c.startswith(('unsigned ', 'signed ')):
        return 'int'
    if re.fullmatch(r'zT\w+|zE\w+|oT\w+|oE\w+|e\w+', c):   # typedefs of enums of the engine (zTRnd_AlphaBlendFunc)
        return 'int'
    return 'struct'


def parse_function(sig):
    text = sig.strip()
    access = None
    m = re.match(r'(public|protected|private):\s+', text)
    if m:
        access = m[1]
        text = text[m.end():]
    quals = []
    while True:
        m = re.match(r'(static|virtual)\s+', text)
        if not m:
            break
        quals.append(m[1])
        text = text[m.end():]
    conv, ci = None, -1
    for c in CONVS:
        i = _find_depth0(text, c)
        if i >= 0 and (ci < 0 or i < ci):
            conv, ci = c, i
    if conv is None:
        return None
    ret = text[:ci].strip()
    rest = text[ci+len(conv):].strip()
    const = False
    if rest.endswith(' const'):
        const = True
        rest = rest[:-6].rstrip()
    if not rest.endswith(')'):
        return None
    depth, open_at = 0, -1
    for i in range(len(rest)-1, -1, -1):
        if rest[i] == ')':
            depth += 1
        elif rest[i] == '(':
            depth -= 1
            if depth == 0:
                open_at = i
                break
    if open_at < 0:
        return None
    qname = rest[:open_at].strip()
    args_raw = rest[open_at+1:-1].strip()
    args = [] if args_raw in ('', 'void') else _split_depth0(args_raw)
    cls, method = None, qname
    parts = _split_depth0(qname.replace('::', '\x00'), '\x00')
    if len(parts) > 1:
        cls, method = '::'.join(parts[:-1]), parts[-1]
    conv = conv[2:]
    if 'static' in quals and conv == 'thiscall':
        conv = 'cdecl'
    return {'name': qname, 'class': cls, 'method': method, 'conv': conv, 'access': access,
            'virtual': 'virtual' in quals, 'static': 'static' in quals, 'const': const,
            'ret': clean_type(ret), 'ret_kind': kind_of_type(ret) if ret else 'ctor',
            'args': [clean_type(a) for a in args], 'arg_kinds': [kind_of_type(a) for a in args]}


def parse_names(path):
    functions, data, vtables = {}, {}, {}
    entries = []
    for line in path.read_text(encoding='latin-1').splitlines():
        m = re.match(r'\s*0x([0-9A-Fa-f]+)\s+(.+?)\s*$', line)
        if m:
            entries.append((int(m[1], 16), m[2]))
    imports = [a for a, n in entries if n.startswith(('__imp_', '&'))]
    code_end = min(imports) if imports else 0xFFFFFFFF
    for addr, name in entries:
        if name.startswith('<') or 'RTTI' in name or name.startswith(('__real@', '??_C@', '__imp_', '&')):
            continue
        vt = re.fullmatch(r"const (.+?)::`vftable'(?:\{for `(.+?)'\})?", name)
        if vt:
            vtables[addr] = {'class': vt[1], 'for': vt[2]}
            continue
        f = parse_function(name) if '(' in name else None
        if f:
            f['sig'] = name
            functions[addr] = f
            continue
        dec = re.fullmatch(r'_(\w+?)@(\d+)', name)
        if dec and addr < code_end:
            functions[addr] = {'name': dec[1], 'class': None, 'method': dec[1], 'conv': 'stdcall', 'access': None,
                               'virtual': False, 'static': False, 'const': False, 'ret': '?', 'ret_kind': 'int',
                               'args': [f'?{i}' for i in range(int(dec[2])//4)],
                               'arg_kinds': ['int'] * (int(dec[2])//4), 'sig': name, 'import': True}
            continue
        if re.fullmatch(r'_\w+', name) and addr < code_end:
            functions[addr] = {'name': name[1:], 'class': None, 'method': name[1:], 'conv': 'cdecl', 'access': None,
                               'virtual': False, 'static': False, 'const': False, 'ret': '?', 'ret_kind': 'int',
                               'args': None, 'arg_kinds': None, 'sig': name, 'crt': True}
            continue
        st = re.fullmatch(r'(?:(?:public|protected|private): )?(static )?(.+?)\s*([\w:<>,` \']+?)$', name)
        data[addr] = {'name': (st[3] if st else name).strip(), 'type': clean_type(st[2]) if st else '', 'decl': name}
    starts = sorted(set(functions) | set(data) | set(vtables))
    for addr, f in functions.items():
        i = bisect.bisect_right(starts, addr)
        f['size'] = (starts[i] - addr) if i < len(starts) else 0
    return functions, data, vtables


# ----------------------------------------------------------------------------------------------------------------------
# parsing of the class declarations (API/*.h): the fields and their offsets

FIELD = re.compile(r'^\s*(?P<type>[^;(){}=]+?)\s*\b(?P<name>\w+)\s*(?P<arr>(?:\[[^\]]*\])*)\s*(?::\s*(?P<bits>\d+))?\s*;'
                   r'\s*//\s*sizeof\s+(?P<size>[0-9A-Fa-f]+)h\s+offset\s+(?P<off>[0-9A-Fa-f]+h|bit)')
SIZE_ONLY = re.compile(r'^\s*//\s*sizeof\s+([0-9A-Fa-f]+)h\s*$')
SCOPE = re.compile(r'^\s*(?:typedef\s+)?(?P<kind>class|struct|union|enum)\s*(?P<name>\w+)?\s*(?::\s*(?P<bases>[^{]+?))?\s*\{')
UNIT = {'char': 1, 'unsigned char': 1, 'signed char': 1, 'bool': 1, 'short': 2, 'unsigned short': 2, 'zBYTE': 1, 'zWORD': 2}


def parse_headers(api_dir):
    classes = {}
    for header in sorted(api_dir.glob('*.h')):
        stack = []           # (kind, qualified name or None, depth at which it opened)
        depth = 0
        pending = None
        for line_no, line in enumerate(header.read_text(encoding='latin-1').splitlines(), 1):
            code = line.split('//', 1)[0]
            m = SIZE_ONLY.match(line)
            if m:
                pending = int(m[1], 16)
            s = SCOPE.match(code)
            if s and code.count('{') > code.count('}'):
                kind, name = s['kind'], s['name']
                owner = next((n for k, n, _ in reversed(stack) if k in ('class', 'struct') and n), None)
                if kind in ('class', 'struct') and name:
                    q = f'{owner}::{name}' if owner else name
                    bases = [re.sub(r'^(?:public|protected|private|virtual)\s+', '', b.strip())
                             for b in _split_depth0(s['bases'] or '')]
                    if q not in classes:
                        classes[q] = {'name': q, 'bases': [b for b in bases if b], 'size': pending,
                                      'fields': [], 'header': header.name, 'line': line_no}
                    stack.append((kind, q, depth))
                else:
                    stack.append((kind, None, depth))      # an enum, an anonymous union/struct: the fields go up
                pending = None
                depth += code.count('{') - code.count('}')
                continue
            if code.strip().startswith(('class ', 'struct ')) and ';' not in code and '{' not in code:
                # the brace is on the next line; rare in these headers
                pass
            f = FIELD.match(line)
            if f and stack:
                owner = next((n for k, n, _ in reversed(stack) if k in ('class', 'struct') and n), None)
                inside_enum = stack[-1][0] == 'enum'
                if owner and not inside_enum:
                    cls = classes[owner]
                    raw_type = f['type'].strip()
                    raw_type = re.sub(r'^(?:mutable|static|const)\s+', '', raw_type)
                    entry = {'name': f['name'], 'type': clean_type(raw_type), 'kind': kind_of_type(raw_type),
                             'size': int(f['size'], 16)}
                    if f['arr']:
                        entry['array'] = f['arr']
                    if f['off'] == 'bit':
                        entry['bits'] = int(f['bits'] or entry['size'])
                        unit = UNIT.get(clean_type(raw_type), 4)
                        prev = cls['fields'][-1] if cls['fields'] else None
                        if prev and 'bits' in prev and prev.get('unit') == unit and prev['bit'] + prev['bits'] + entry['bits'] <= unit*8:
                            entry['offset'], entry['bit'] = prev['offset'], prev['bit'] + prev['bits']
                        elif prev and prev.get('offset') is not None:
                            end = prev['offset'] + (prev.get('unit', 0) if 'bits' in prev else prev['size'])
                            entry['offset'], entry['bit'] = end, 0
                        else:
                            entry['offset'], entry['bit'] = None, 0
                        entry['unit'] = unit
                        entry['size'] = unit
                    else:
                        entry['offset'] = int(f['off'][:-1], 16)
                    cls['fields'].append(entry)
            depth += code.count('{') - code.count('}')
            while stack and depth <= stack[-1][2]:
                stack.pop()
    return classes


def parse_globals(path):
    out = {}
    if not path.exists():
        return out
    for m in re.finditer(r'^\s*(?P<type>[\w:<>*& ]+?)\s*&?\s*(?P<name>\w+)\s*=\s*\*\s*\([^)]*\)\s*(?P<addr>0x[0-9A-Fa-f]+)\s*;',
                         path.read_text(encoding='latin-1'), re.M):
        out[int(m['addr'], 16)] = {'name': m['name'], 'type': clean_type(m['type'].rstrip('&').strip())}
    return out


def build_engine(api_root, key):
    base = api_root / 'ZenGin' / ENGINES[key]
    functions, data, vtables = parse_names(base / 'Names.txt')
    classes = parse_headers(base / 'API')
    globals_ = parse_globals(base / 'GlobalVariables.h')
    for addr, g in globals_.items():
        data.setdefault(addr, {}).update({'name': g['name'], 'type': g['type'], 'global': True})
    return {'schema': SCHEMA, 'engine': key, 'source': API_URL, 'source_dir': ENGINES[key],
            'functions': {f'0x{a:08x}': f for a, f in sorted(functions.items())},
            'data': {f'0x{a:08x}': d for a, d in sorted(data.items())},
            'vtables': {f'0x{a:08x}': v for a, v in sorted(vtables.items())},
            'classes': classes}


# ----------------------------------------------------------------------------------------------------------------------
# the database as a library (worklist.py, audit tools)

class EngineDb:
    def __init__(self, data):
        self.engine = data['engine']
        self.functions = {int(k, 16): v for k, v in data['functions'].items()}
        self.data = {int(k, 16): v for k, v in data['data'].items()}
        self.vtables = {int(k, 16): v for k, v in data['vtables'].items()}
        self.classes = data['classes']
        self._starts = sorted(self.functions)
        self._dstarts = sorted(self.data)
        self._by_name = {}
        for a, f in self.functions.items():
            self._by_name.setdefault(f['name'].lower(), []).append(a)
        self._class_lower = {k.lower(): k for k in self.classes}
        self._vtable_of = {}
        for a, v in sorted(self.vtables.items()):
            if not v.get('for'):
                self._vtable_of[v['class']] = a
        for a, v in sorted(self.vtables.items()):
            # multiple inheritance: the table of the first base is at offset 0 (zCView: {for zCViewBase})
            self._vtable_of.setdefault(v['class'], a)

    @staticmethod
    def load(engine='G2A', out=DEFAULT_OUT):
        key = ALIASES.get(engine.upper(), engine.upper())
        path = Path(out) / f'{key}.json'
        if not path.exists():
            raise FileNotFoundError(f'{path} does not exist: run  python tools/ikarus/enginedb.py build --fetch')
        return EngineDb(json.loads(path.read_text(encoding='utf-8')))

    def function_at(self, addr):
        """(the function, the offset in it) for an address of code; None when it is not in a known function"""
        i = bisect.bisect_right(self._starts, addr) - 1
        if i < 0:
            return None, 0
        start = self._starts[i]
        f = self.functions[start]
        off = addr - start
        if off and f.get('size') and off >= f['size']:
            return None, 0
        return dict(f, address=start), off

    def data_at(self, addr):
        if addr in self.data:
            return dict(self.data[addr], address=addr), 0
        if addr in self.vtables:
            v = self.vtables[addr]
            return {'name': f"{v['class']}::vftable" + (f" for {v['for']}" if v.get('for') else ''), 'address': addr}, 0
        i = bisect.bisect_right(self._dstarts, addr) - 1
        if i >= 0 and addr - self._dstarts[i] < 64:
            return dict(self.data[self._dstarts[i]], address=self._dstarts[i]), addr - self._dstarts[i]
        return None, 0

    def describe(self, addr):
        f, off = self.function_at(addr)
        if f:
            return f"{f['name']}+0x{off:x}" if off else f['name']
        d, off = self.data_at(addr)
        if d:
            return f"{d['name']}+0x{off:x}" if off else d['name']
        return ''

    def find(self, name):
        return sorted(self._by_name.get(name.lower(), []))

    def cls(self, name):
        key = self._class_lower.get(name.lower())
        return self.classes.get(key) if key else None

    def vtable(self, cls):
        return self._vtable_of.get(cls)

    def class_of_vtable(self, addr):
        v = self.vtables.get(addr)
        return v['class'] if v else None

    def layout(self, name, _seen=None):
        """All the fields of a class with the fields of its bases (the first base is at 0), sorted by the offset"""
        c = self.cls(name)
        if c is None:
            return []
        _seen = _seen or set()
        if c['name'] in _seen:
            return []
        _seen.add(c['name'])
        out = []
        for i, b in enumerate(c['bases']):
            if i == 0:
                out += [dict(f, owner=f.get('owner', b)) for f in self.layout(b, _seen)]
        out += [dict(f, owner=c['name']) for f in c['fields']]
        return sorted((f for f in out if f.get('offset') is not None), key=lambda f: (f['offset'], f.get('bit', 0)))

    def field(self, name, offset):
        """The fields that cover an offset of an object of the class (several for bit fields)"""
        hits = [f for f in self.layout(name) if f['offset'] <= offset < f['offset'] + max(1, f['size'])]
        return hits

    # a skeleton of the C++ handler, in the style of directmemory*.cpp
    CPP = {'int': 'int32_t', 'ptr': 'ptr32_t', 'float': 'float', 'string': 'std::string', 'cstring': 'ptr32_t',
           'struct': 'ptr32_t', 'wide': 'int32_t'}

    def stub(self, addr, hook=False):
        f, off = self.function_at(addr)
        if f is None:
            return f'// 0x{addr:08x}: not a known function of {self.engine}'
        const = re.sub(r'\W', '_', (f['class'] + '__' if f['class'] else '') + f['method']).upper().strip('_')
        if hook or off:
            return self._hook_stub(f, off, addr, const)
        lines = [f"// 0x{addr:08x}  {f['sig']}"]
        notes, params = [], []
        conv = f['conv']
        if conv == 'thiscall':
            params.append(f"ptr32_t self /* {f['class']}* */")
        if f['ret_kind'] == 'struct' or f['ret_kind'] == 'string' and not f['ret'].endswith(('*', '&')):
            params.append(f"ptr32_t ret /* {f['ret']}* : the caller's memory for the result, return it in EAX */")
            notes.append('the result is a structure by value: MSVC passes a hidden pointer after this')
        if f['ret'] == 'zSTRING':
            params[-1] = "ptr32_t ret /* zSTRING*: memAssignString(*mem32.deref<zString>(ret), ...); return ret */"
        for i, (t, k) in enumerate(zip(f['args'] or [], f['arg_kinds'] or [])):
            ct = self.CPP.get(k, 'int32_t')
            if k == 'struct':
                c = self.cls(t)
                slots = (c['size'] + 3)//4 if c and c.get('size') else '?'
                notes.append(f'argument {i} ({t}) is passed by value: {slots} stack slot(s), not a pointer; pop them as int32_t')
                ct = 'int32_t'
            if k == 'wide':
                notes.append(f'argument {i} ({t}) takes 2 stack slots: declare two int32_t')
            if k == 'string':
                t += ': Cpu32 reads the zSTRING behind the pointer'
            params.append(f'{ct} a{i} /* {t} */')
        if f['args'] is None:
            notes.append('a function of the C library: the arguments are not in the database, see its documentation')
        ret = {'void': '', 'ctor': ' -> ptr32_t', 'float': ' -> float', 'int': ' -> int32_t', 'ptr': ' -> ptr32_t',
               'string': ' -> ptr32_t', 'cstring': ' -> ptr32_t', 'struct': ' -> ptr32_t', 'wide': ' -> int32_t'}[f['ret_kind']]
        reg = {'thiscall': 'register_thiscall', 'stdcall': 'register_stdcall', 'cdecl': 'register_cdecl',
               'fastcall': 'register_thiscall'}[conv]
        if conv == 'fastcall':
            notes.append('__fastcall: the first two arguments are in ECX and EDX; Cpu32 has no adapter for EDX yet')
        lines += [f'//   {n}' for n in notes]
        lines.append(f'static constexpr uint32_t {const} = 0x{addr:08X};')
        lines.append(f'cpu.{reg}({const}, [this]({", ".join(params)}){ret} {{')
        if f['class'] in ('oCNpc', 'oCItem') or (f['class'] or '').startswith(('oCNpc', 'oCItem')):
            lines.append('  // self is a handle of InstanceHandles: handles.instanceAt(self) -> zenkit::INpc/IItem, user_ptr -> Npc/Item')
        lines.append('  // TODO: the effect in OpenGothic')
        if ret:
            lines.append('  return 0;')
        lines.append('  });')
        return '\n'.join(lines)

    def _hook_stub(self, f, off, addr, const):
        lines = [f"// hook at 0x{addr:08x} = {f['name']}+0x{off:x}", f"//   {f['sig']}"]
        if off == 0:
            lines.append('//   the entry of the function: ' +
                         ('ECX = this; ' if f['conv'] == 'thiscall' else '') +
                         ', '.join(f'ESP+{4*(i+1)} = {t}' for i, t in enumerate(f['args'] or [])) +
                         ('' if f['args'] else '(no stack arguments)'))
        else:
            lines.append('//   inside the function: the registers depend on the code; '
                         'disassemble the original executable at this address (worklist.py --exe does it)')
        lines.append(f'static constexpr uint32_t {const}_HOOK = 0x{addr:08X};   // add to hookaddresses.h and isHookAnswered')
        lines.append('// where OpenGothic does the same thing:')
        lines.append('DirectMemory::HookRegs regs;')
        if f['conv'] == 'thiscall' and off == 0:
            lines.append(f"regs.ecx  = ptrOf(...);            // {f['class']}*")
        for i, t in enumerate((f['args'] or []) if off == 0 else []):
            lines.append(f'regs.args.push_back(...);          // ESP+{4*(i+1)}: {t}')
        lines.append(f'fireHook({const}_HOOK, regs);       // then read back what the script left in regs')
        return '\n'.join(lines)


# ----------------------------------------------------------------------------------------------------------------------

def parse_addr(text):
    text = text.strip().lower()
    if text.startswith('0x'):
        return int(text, 16)
    if re.fullmatch(r'[0-9a-f]+', text) and re.search(r'[a-f]', text):
        return int(text, 16)
    return int(text)


def cmd_build(args):
    api = Path(args.api)
    if not (api / 'ZenGin').is_dir():
        if not args.fetch:
            sys.exit(f'{api} is not a checkout of gothic-api; add --fetch or --api <dir>')
        api.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'clone', '--depth', '1', API_URL, str(api)], check=True)
    rev = subprocess.run(['git', '-C', str(api), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for key in (args.engine or ENGINES):
        db = build_engine(api, key)
        db['revision'] = rev
        path = out / f'{key}.json'
        path.write_text(json.dumps(db, separators=(',', ':')), encoding='utf-8')
        fields = sum(len(c['fields']) for c in db['classes'].values())
        print(f"{key}: {len(db['functions'])} functions, {len(db['classes'])} classes ({fields} fields), "
              f"{len(db['data'])} data, {len(db['vtables'])} vtables -> {path}")


def cmd_lookup(args):
    db = EngineDb.load(args.engine, args.out)
    for a in args.address:
        addr = parse_addr(a)
        f, off = db.function_at(addr)
        if f:
            where = f' + 0x{off:x} (inside the function)' if off else ''
            print(f"0x{addr:08x}  {f['sig']}{where}  [{f['conv']}]")
            continue
        d, off = db.data_at(addr)
        if d:
            print(f"0x{addr:08x}  data {d.get('decl') or d['name']}" + (f' + 0x{off:x}' if off else ''))
            continue
        print(f'0x{addr:08x}  unknown in {db.engine}')


def cmd_find(args):
    db = EngineDb.load(args.engine, args.out)
    pat = re.compile(args.pattern, re.I)
    for a, f in sorted(db.functions.items()):
        if pat.search(f['sig']):
            print(f"0x{a:08x}  {f['sig']}")


def cmd_class(args):
    db = EngineDb.load(args.engine, args.out)
    c = db.cls(args.name)
    if c is None:
        sys.exit(f'no class {args.name} in {db.engine}')
    size = f"0x{c['size']:x}" if c.get('size') else '?'
    vt = db.vtable(c['name'])
    print(f"class {c['name']} : {', '.join(c['bases']) or '-'}   sizeof {size}   vtable "
          f"{'0x%08x' % vt if vt else '-'}   ({c['header']}:{c['line']})")
    for f in db.layout(c['name']):
        bit = f" bit {f['bit']}:{f['bits']}" if 'bits' in f else ''
        print(f"  +0x{f['offset']:04x} {f['size']:5}  {f['type']} {f['name']}{f.get('array', '')}{bit}   ({f['owner']})")


def cmd_field(args):
    db = EngineDb.load(args.engine, args.out)
    off = parse_addr(args.offset)
    hits = db.field(args.cls, off)
    if not hits:
        print(f'{args.cls}+0x{off:x}: no field')
    for f in hits:
        inner = off - f['offset']
        print(f"{args.cls}+0x{off:x} = {f['owner']}.{f['name']}{f.get('array', '')}" +
              (f' + {inner}' if inner else '') + f"   ({f['type']}, {f['size']} bytes)")


def cmd_stub(args):
    db = EngineDb.load(args.engine, args.out)
    for a in args.address:
        print(db.stub(parse_addr(a), hook=args.hook))
        print()


def cmd_symbolize(args):
    db = EngineDb.load(args.engine, args.out)
    text = Path(args.file).read_text(encoding='utf-8', errors='replace') if args.file != '-' else sys.stdin.read()

    def repl(m):
        try:
            addr = int(m[0], 16) if m[0].lower().startswith('0x') else int(m[0])
        except ValueError:
            return m[0]
        if not 0x400000 <= addr < 0x1000000:
            return m[0]
        name = db.describe(addr)
        return f'{m[0]} <{name}>' if name else m[0]
    sys.stdout.write(re.sub(r'\b0x[0-9a-fA-F]{6,8}\b|\b(?:4|5|6|7|8|9|1[0-6])\d{6}\b', repl, text))


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--engine', default='G2A', help='G1, G1A, G2, G2A (default; Gothic 2 NotR)')
    ap.add_argument('--out', default=str(DEFAULT_OUT), help='the directory of the databases')
    sub = ap.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build', help='convert gothic-api into JSON databases')
    b.add_argument('--api', default=str(DEFAULT_API))
    b.add_argument('--fetch', action='store_true', help='clone gothic-api when --api does not exist')
    b.add_argument('--only', dest='engine', action='append', choices=list(ENGINES))
    l = sub.add_parser('lookup', help='the names of addresses')
    l.add_argument('address', nargs='+')
    fd = sub.add_parser('find', help='the functions whose signature matches a regular expression')
    fd.add_argument('pattern')
    c = sub.add_parser('class', help='the layout of a class')
    c.add_argument('name')
    f = sub.add_parser('field', help='the field at an offset of a class')
    f.add_argument('cls')
    f.add_argument('offset')
    s = sub.add_parser('stub', help='a C++ skeleton of a handler')
    s.add_argument('address', nargs='+')
    s.add_argument('--hook', action='store_true', help='a hook (HookEngine), not a call')
    y = sub.add_parser('symbolize', help='add the names to the addresses of a text (a log, a CSV)')
    y.add_argument('file', help="a file or '-'")
    args = ap.parse_args()
    if args.cmd == 'build' and args.engine is None:
        args.engine = list(ENGINES)
    {'build': cmd_build, 'lookup': cmd_lookup, 'find': cmd_find, 'class': cmd_class, 'field': cmd_field,
     'stub': cmd_stub, 'symbolize': cmd_symbolize}[args.cmd](args)


if __name__ == '__main__':
    main()
