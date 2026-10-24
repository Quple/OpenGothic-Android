#!/usr/bin/env python3
"""Extract GOTHIC.DAT from an uncompressed Gothic VDF/MOD, without executing it."""
import argparse
import struct
from pathlib import Path


def extract(archive, output):
    with archive.open('rb') as stream:
        header = stream.read(296)
        if len(header) != 296 or not header[256:272].startswith(b'PSVDSC_V'):
            raise ValueError('Not a Gothic VDF/MOD archive')
        count, _, _, _, catalog, flags = struct.unpack_from('<6I', header, 272)
        if flags != 80:
            raise ValueError(f'Unsupported VDF volume flags {flags}; use ZenKit to extract compressed volumes')
        length = archive.stat().st_size
        catalog = catalog or 296
        if catalog + count * 80 > length:
            raise ValueError('VDF catalog extends past the archive')
        stream.seek(catalog)
        entries = stream.read(count * 80)
        matches = []
        for i in range(count):
            entry = entries[i*80:(i+1)*80]
            name = entry[:64].rstrip(b' \0').upper()
            offset, size, kind, _ = struct.unpack_from('<4I', entry, 64)
            if name == b'GOTHIC.DAT' and not kind & 0x80000000:
                matches.append((offset, size))
        if len(matches) != 1:
            raise ValueError(f'Expected exactly one GOTHIC.DAT, found {len(matches)}')
        offset, size = matches[0]
        if offset + size > length:
            raise ValueError('GOTHIC.DAT extends past the archive')
        stream.seek(offset)
        data = stream.read(size)
    if output.exists():
        raise ValueError(f'Output already exists: {output}')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(f'{output}: {len(data)} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    extract(args.archive, args.output)
