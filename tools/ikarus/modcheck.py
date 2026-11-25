#!/usr/bin/env python3
"""One command for a mod that uses Ikarus/LeGo: what does it need from OpenGothic, and what is missing.

    python tools/ikarus/modcheck.py --game "C:/Games/TheChroniclesOfMyrtana" --ini TheChroniclesOfMyrtana.ini \
        [--run "wait 30; comcheck test ikarus; wait 10"] [--save 3] [--timeout 400] [--out build-ikarus/<name>]

Steps (each one is skipped when its result is there and --fresh is not given):
  1. the scripts: GOTHIC.DAT from the newest volume of Data/ (and Data/ModVDF/) that has one, or _work/data/scripts/_compiled
  2. the engine database (enginedb.py build --fetch), once for all the mods
  3. the static audit (analyze.py --dat ...): every call of the engine, hook, external and its binding in C++
  4. with --run: the game itself (build-desktop/opengothic/Gothic2Notr.exe or --og), without a person: -nomenu (or
     -save N), the commands of --run, the report autorun-report.json and the gaps that the game recorded
  5. the work list (worklist.py): one task per gap, by priority, with the signature, the scripts and a C++ skeleton
The summary is printed and written to <out>/SUMMARY.md. Nothing is changed in the folder of the game.
"""
import argparse
import json
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
DEFAULT_OG = REPO / 'build-desktop' / 'opengothic' / 'Gothic2Notr.exe'


def volume_entries(path):
    """(name, offset, size) of the files of an uncompressed VDF/MOD volume, and its timestamp"""
    with path.open('rb') as s:
        header = s.read(296)
        if len(header) != 296 or not header[256:272].startswith(b'PSVDSC_V'):
            return None, []
        count, _, stamp, _, catalog, _ = struct.unpack_from('<6I', header, 272)
        s.seek(catalog or 296)
        raw = s.read(count * 80)
    out = []
    for i in range(len(raw) // 80):
        e = raw[i*80:(i+1)*80]
        name = e[:64].rstrip(b' \0').decode('latin-1').upper()
        off, size, kind, _ = struct.unpack_from('<4I', e, 64)
        if not kind & 0x80000000:
            out.append((name, off, size))
    return stamp, out


def find_dat(game):
    """The GOTHIC.DAT that the game would load: the newest volume wins, as in the VDF system of Gothic"""
    best = None
    for folder in (game / 'Data', game / 'Data' / 'ModVDF', game / 'data', game / 'data' / 'modvdf'):
        if not folder.is_dir():
            continue
        for vol in list(folder.glob('*.mod')) + list(folder.glob('*.MOD')) + list(folder.glob('*.vdf')):
            try:
                stamp, entries = volume_entries(vol)
            except OSError:
                continue
            for name, off, size in entries:
                if name == 'GOTHIC.DAT' and (best is None or stamp > best[0]):
                    best = (stamp, vol, off, size)
    if best:
        return best[1], best[2], best[3]
    for loose in game.glob('_work/[Dd]ata/[Ss]cripts/_compiled/GOTHIC.DAT'):
        return loose, 0, loose.stat().st_size
    return None, 0, 0


def engine_of(game):
    system = game / 'system' if (game / 'system').is_dir() else game / 'System'
    if (system / 'Gothic2.exe').exists() or (system / 'GOTHIC2.EXE').exists():
        return 'G2A', next(iter(system.glob('[Gg]othic2.exe')), None)
    if (system / 'Gothic.exe').exists() or (system / 'GOTHIC.EXE').exists():
        return 'G1', next(iter(system.glob('[Gg]othic.exe')), None)
    return 'G2A', None


def run(cmd, **kw):
    print('  $ ' + ' '.join(str(c) for c in cmd), flush=True)
    return subprocess.run([str(c) for c in cmd], **kw)


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', type=Path, required=True, help='the folder of the installation (with Data/ and System/)')
    ap.add_argument('--ini', help='the ini of the mod in System/ (the -game: of OpenGothic)')
    ap.add_argument('--dat', type=Path, help='GOTHIC.DAT, instead of searching the volumes')
    ap.add_argument('--engine', help='G1 or G2A; by default from the executable in System/')
    ap.add_argument('--out', type=Path)
    ap.add_argument('--run', help='the commands of an unattended run of the game (see common/utils/autorun.h)')
    ap.add_argument('--save', type=int, help='load this save slot instead of a new game')
    ap.add_argument('--timeout', type=int, default=600, help='seconds for the run of the game')
    ap.add_argument('--og', type=Path, default=DEFAULT_OG, help='Gothic2Notr.exe of OpenGothic')
    ap.add_argument('--gaps', type=Path, nargs='*', default=[], help='more records: log.txt of a phone, ikarus-gaps.json')
    ap.add_argument('--fresh', action='store_true', help='do every step again')
    ap.add_argument('--only', choices=['dat', 'audit', 'run', 'worklist'], help='stop after this step')
    args = ap.parse_args()

    game = args.game.resolve()
    name = re.sub(r'\.ini$', '', args.ini or game.name, flags=re.I)
    out = (args.out or REPO / 'build-ikarus' / re.sub(r'[^\w.-]+', '_', name)).resolve()
    out.mkdir(parents=True, exist_ok=True)
    engine, exe = engine_of(game)
    engine = args.engine or engine
    py = sys.executable
    summary = [f'# {name}', '', f'Game: `{game}`, engine {engine}.']

    # 1. the scripts
    dat = out / 'GOTHIC.DAT'
    if args.fresh and dat.exists():
        dat.unlink()
    if not dat.exists():
        if args.dat:
            shutil.copyfile(args.dat, dat)
            src = args.dat
        else:
            vol, off, size = find_dat(game)
            if vol is None:
                sys.exit(f'no GOTHIC.DAT in {game}/Data (a compressed volume? extract it with ZenKit and give --dat)')
            with vol.open('rb') as s:
                s.seek(off)
                dat.write_bytes(s.read(size))
            src = vol
        print(f'1. scripts: {src} -> {dat} ({dat.stat().st_size} bytes)')
    else:
        print(f'1. scripts: {dat} (kept)')
    raw = dat.read_bytes()
    uses = [w for w in ('IKARUS_VERSION', 'LEGO_VERSION', 'MEM_INITALL', 'HOOKENGINEF') if w.encode() in raw]
    summary.append(f'Scripts: `{dat}`; libraries: {", ".join(uses) or "none (no Ikarus/LeGo)"}.')
    if args.only == 'dat':
        return

    # 2. the engine database
    db = REPO / 'build-ikarus' / 'enginedb' / f'{engine}.json'
    if not db.exists():
        print('2. engine database')
        run([py, HERE / 'enginedb.py', 'build', '--fetch'], check=True)
    else:
        print(f'2. engine database: {db} (kept)')

    # 3. the static audit
    audit = out / 'audit'
    if args.fresh or not (audit / 'addresses.csv').exists() or (audit / 'addresses.csv').stat().st_mtime < dat.stat().st_mtime:
        print('3. audit')
        with (out / 'audit.log').open('w', encoding='utf-8') as log:
            run([py, HERE / 'analyze.py', '--dat', dat, '--repo', REPO, '--out', audit], check=True, stdout=log,
                stderr=subprocess.STDOUT)
    else:
        print(f'3. audit: {audit} (kept; the C++ changed? add --fresh)')
    summary_json = json.loads((audit / 'summary.json').read_text(encoding='utf-8'))
    summary.append('Static audit (call sites of the engine, by status): ' +
                   ', '.join(f'{k} {v}' for k, v in sorted(summary_json.get('native_status', {}).items())) + '.')
    if args.only == 'audit':
        return

    # 4. the game itself
    gaps = list(args.gaps)
    if args.run is not None:
        og = args.og.resolve()
        if not og.exists():
            sys.exit(f'{og} does not exist: build the desktop game (docs/IKARUS_AUTOMATION.md) or give --og')
        rundir = out / 'run'
        if rundir.exists():
            for f in ('autorun-report.json', 'ikarus-gaps.json', 'log.txt', 'crash.log', 'crash.dmp', 'crash.json'):
                (rundir / f).unlink(missing_ok=True)
        rundir.mkdir(exist_ok=True)
        # the mods of the repository (devtools: comcheck) are in the working directory of the game
        shutil.copytree(REPO / 'mods', rundir / 'mods', dirs_exist_ok=True)
        cmd = [og, '-g', str(game)]
        if args.ini:
            cmd.append('-game:' + args.ini)
        cmd += ['-save', str(args.save)] if args.save is not None else ['-nomenu']
        cmd += ['-window', '-noerrors', '-autorun', args.run, '-autoexit', str(args.timeout)]
        print('4. the game')
        t = time.time()
        try:
            r = run(cmd, cwd=rundir, timeout=args.timeout + 60)
            code = r.returncode
        except subprocess.TimeoutExpired:
            code = 'killed after the timeout'
        report = rundir / 'autorun-report.json'
        print(f'   ended ({code}) after {int(time.time()-t)} s')
        if report.exists():
            rep = json.loads(report.read_text(encoding='utf-8', errors='replace'))
            gaps.append(report)
            fails = [c['text'] for c in rep.get('console', []) if c['text'].startswith('FAIL')]
            summary.append(f"Run: {rep.get('result')} in {rep.get('seconds')} s, world {rep.get('world', '-')}, "
                           f"{rep.get('logErrors')} errors in the log, {len(rep['gaps']['gaps'])} gaps "
                           f"({rep.get('gapsTotal')} hits), {len(fails)} failed checks.")
            for f in fails[:20]:
                summary.append(f'- {f}')
        else:
            # the game ended without its report: a crash or a hang. The IKGAP lines of log.txt are still there; the
            # tools of the dialogs say which one was running (dialogtest_last.txt)
            last = (rundir / 'dialogtest_last.txt').read_text(encoding='utf-8', errors='replace').strip() \
                if (rundir / 'dialogtest_last.txt').exists() else ''
            tail = []
            if (rundir / 'log.txt').exists():
                tail = (rundir / 'log.txt').read_text(encoding='utf-8', errors='replace').splitlines()[-40:]
                gaps.append(rundir / 'log.txt')
            crash = rundir / 'crash.log'
            dump = rundir / 'crash.dmp'
            if isinstance(code, int):
                code = f'{code:#x}' if code > 255 or code < 0 else str(code)
            what = f'the game ended without a report (exit {code})' + (f', in the dialog {last}' if last else '')
            summary.append(f'Run: {what}; see {rundir / "log.txt"}' + (f' and {crash}' if crash.exists() else '') +
                           (f', the minidump {dump} (Visual Studio or WinDbg with the .pdb of the build)' if dump.exists() else '') +
                           ('. Go on with `dlgall resume` (the dialog that crashed is left).' if last else '.'))
            record = {'kind': 'crash', 'name': what, 'count': 1, 'stack': [last] if last else [],
                      'detail': '\n'.join(tail[-25:]) +
                                ('\n--- crash.log\n' + crash.read_text(encoding='utf-8', errors='replace')[:4000]
                                 if crash.exists() else '')}
            (rundir / 'crash.json').write_text(json.dumps({'gaps': [record]}, indent=1), encoding='utf-8')
            gaps.append(rundir / 'crash.json')
    if args.only == 'run':
        print('\n'.join(summary))
        return

    # 5. the work list
    print('5. work list')
    wl = out / 'worklist'
    cmd = [py, HERE / 'worklist.py', '--audit', audit, '--dat', dat, '--engine', engine, '--out', wl]
    if gaps:
        cmd += ['--gaps'] + gaps
    if exe:
        cmd += ['--exe', exe]
    r = run(cmd, capture_output=True, text=True, errors='replace')
    print(r.stdout[-4000:])
    if r.returncode:
        print(r.stderr[-4000:])
        sys.exit(r.returncode)
    tasks = json.loads((wl / 'tasks.json').read_text(encoding='utf-8'))
    by_kind = {}
    for t in tasks:
        by_kind[t['kind']] = by_kind.get(t['kind'], 0) + 1
    summary.append(f"Work list: {len(tasks)} tasks (" + ', '.join(f'{k} {v}' for k, v in sorted(by_kind.items())) +
                   f"), `{wl / 'INDEX.md'}`.")
    summary += ['', '| # | task | hits | reach |', '|---|---|---|---|']
    summary += [f"| {t['n']} | [{t['title']}](worklist/{t['file']}) | {t['hits']} | {t['reach']} |" for t in tasks[:25]]
    (out / 'SUMMARY.md').write_text('\n'.join(summary) + '\n', encoding='utf-8')
    print('\n'.join(summary[:12]))
    print(f'\nSummary: {out / "SUMMARY.md"}')


if __name__ == '__main__':
    main()
