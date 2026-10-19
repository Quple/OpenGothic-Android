#!/usr/bin/env python3
"""
Which parts of Ikarus and LeGo does a mod use, and what of it does OpenGothic not do yet?

OpenGothic runs the scripts of Ikarus and LeGo in its own virtual machine. A script of them calls the functions of the
original engine by the addresses of Gothic2.exe (CALL__thiscall, HookEngine, ...), and the compatibility layer
(common/game/compatibility) answers for the addresses that it knows. This tool lists the addresses that are not known
and the library functions that need them, ordered by how often the mod uses these functions.

  python tools/ikarus/analyze.py --ikarus <Ikarus> --lego <LeGo> --mod <decompiled mod> [--repo <OpenGothic>]

The sources are the repositories https://github.com/Lehona/Ikarus, https://github.com/Lehona/LeGo and, for example,
https://github.com/lmichaelis/tcom-decompilation (The Chronicles of Myrtana).
"""
import argparse, collections, os, re, sys


def read_tree(root):
    out = {}
    for dp, _, fn in os.walk(root):
        for f in fn:
            if f.lower().endswith('.d'):
                p = os.path.join(dp, f)
                out[p] = open(p, encoding='latin-1').read()
    return out


def strip(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


def num(s):
    s = s.strip()
    return int(s, 16) if s.lower().startswith('0x') else int(s)


def functions(text):
    """name -> (parameters, body) of the functions of a file"""
    out = {}
    for m in re.finditer(r'\bfunc\s+\w+\s+([A-Za-z_]\w*)\s*\(', text, flags=re.I):
        i = text.find('{', m.end())
        if i < 0:
            continue
        depth, j = 0, i
        while j < len(text):
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        out[m.group(1).upper()] = text[i:j + 1]
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--ikarus', required=True)
    ap.add_argument('--lego', required=True)
    ap.add_argument('--mod', required=True)
    ap.add_argument('--repo', default=os.path.join(os.path.dirname(__file__), '..', '..'))
    ap.add_argument('--top', type=int, default=40)
    a = ap.parse_args()

    ikarus, lego, mod = read_tree(a.ikarus), read_tree(a.lego), read_tree(a.mod)
    consts = {}
    cre = re.compile(r'\bconst\s+int\s+([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;')
    for src in (ikarus, lego, mod):
        for t in src.values():
            for m in cre.finditer(strip(t)):
                consts[m.group(1).upper()] = num(m.group(2))
    names = {v: k for k, v in consts.items() if v > 0x400000}

    # the addresses that the compatibility layer answers for
    cpp = ''
    cdir = os.path.join(a.repo, 'common', 'game', 'compatibility')
    for f in os.listdir(cdir):
        if f.endswith('.cpp'):
            cpp += open(os.path.join(cdir, f), encoding='utf-8', errors='replace').read()
    cppc = {m.group(1): num(m.group(2)) for m in re.finditer(r'\b(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;', cpp)}
    handled = set()
    for m in re.finditer(r'register_(?:thiscall|cdecl|stdcall|fastcall)\(\s*(\w+)', cpp):
        tok = m.group(1)
        v = num(tok) if re.fullmatch(r'0x[0-9A-Fa-f]+|\d+', tok) else cppc.get(tok)
        if v:
            handled.add(v)

    # functions of the library that are replaced by C++ do not need the engine
    overridden = {m.group(1).upper() for m in re.finditer(r'override_function\(\s*"(\w+)"', cpp)}

    # what the mod calls
    used = collections.Counter()
    for t in mod.values():
        for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', strip(t)):
            used[m.group(1).upper()] += 1

    # library functions that call an engine function
    need = collections.defaultdict(set)
    for src in (ikarus, lego):
        for t in src.values():
            t = strip(t)
            for name, body in functions(t).items():
                local = dict(consts)
                for m in cre.finditer(body):
                    local[m.group(1).upper()] = num(m.group(2))
                for m in re.finditer(r'\bCALL__(?:thiscall|stdcall|cdecl|fastcall)\s*\(([^;]*)\)\s*;', body, flags=re.I | re.S):
                    arg = m.group(1)
                    sw = re.search(r'MEMINT_SwitchG1G2\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', arg, flags=re.I)
                    tok = (sw.group(2) if sw else arg.split(',')[-1]).strip()
                    v = num(tok) if re.fullmatch(r'0x[0-9A-Fa-f]+|\d+', tok) else local.get(tok.upper())
                    if v and v > 0x400000:
                        need[name].add(v)

    print('Engine functions that the library needs and OpenGothic does not answer for,')
    print('by the functions of the library that the mod calls:\n')
    rows = []
    for fn, addrs in need.items():
        miss = sorted(x for x in addrs if x not in handled)
        # CALL_* is the framework that builds the calls, the engine function is the one of its caller
        if miss and used[fn] > 0 and fn not in overridden and not fn.startswith('CALL_'):
            rows.append((used[fn], fn, miss))
    rows.sort(reverse=True)
    for count, fn, miss in rows[:a.top]:
        print(f'{count:6d}  {fn:34s} {", ".join(names.get(x, hex(x)) for x in miss)}')

    hooks = collections.Counter()
    for t in list(mod.values()) + list(lego.values()):
        for m in re.finditer(r'\bHookEngine[FI]?\s*\(\s*(\w+)', strip(t), flags=re.I):
            hooks[m.group(1).upper()] += 1
    print(f'\nHookEngine calls in the mod and in LeGo: {sum(hooks.values())} '
          f'(OpenGothic does not patch the engine: it keeps the hooks that it can answer by an event, see DirectMemory::setupHookEngine)')


if __name__ == '__main__':
    sys.exit(main())
