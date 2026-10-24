#!/usr/bin/env python3
"""Compatibility entry point for the Daedalus call-graph audit.

See audit.py --help. The original --ikarus/--lego/--mod/--repo/--top
arguments are retained; --dat adds authoritative bytecode counts and
--disassemble dumps every function. Reports default to build/ikarus-audit.
"""
from audit import main


if __name__ == '__main__':
    main()
