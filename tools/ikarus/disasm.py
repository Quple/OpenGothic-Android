"""Disassembler of the Daedalus scripts (GOTHIC.DAT), for the functions that the decompilation of a mod lost.

    python tools/ikarus/disasm.py GOTHIC.DAT FUNCTION_NAME [FUNCTION_NAME ...]

The names are not case sensitive. The output is the linear code of the function with the names of the symbols.
"""
import struct
import sys

OPS = {
    0: "add", 1: "sub", 2: "mul", 3: "div", 4: "mod", 5: "or", 6: "andb", 7: "lt", 8: "gt", 9: "movi",
    11: "orr", 12: "and", 13: "lsl", 14: "lsr", 15: "lte", 16: "eq", 17: "neq", 18: "gte",
    19: "addmovi", 20: "submovi", 21: "mulmovi", 22: "divmovi",
    30: "plus", 31: "negate", 32: "not", 33: "cmpl", 45: "nop", 60: "rsr", 61: "bl", 62: "be",
    64: "pushi", 65: "pushv", 67: "pushvi", 70: "movs", 71: "movss", 72: "movvf", 73: "movf",
    74: "movvi", 75: "b", 76: "bz", 80: "gmovi", 245: "pushvv",
}
TYPES = {0: "void", 1: "float", 2: "int", 3: "string", 4: "class", 5: "func", 6: "prototype", 7: "instance"}


class Dat:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        self.p = 0
        self.symbols = []
        self.load()

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def line(self):
        end = self.d.index(b"\n", self.p)
        s = self.d[self.p:end].decode("latin1")
        self.p = end + 1
        return s

    def load(self):
        self.u8()
        count = self.u32()
        self.p += count * 4
        for i in range(count):
            named = self.u32()
            name = self.line() if named else ""
            self.u32()
            props = self.u32()
            n, typ, flags = props & 0xFFF, (props >> 12) & 0xF, (props >> 16) & 0x3F
            for _ in range(5):
                self.u32()
            value = None
            if not (flags & 0x4):
                if typ == 1:
                    self.p += 4 * n
                elif typ == 2:
                    value = [struct.unpack_from("<i", self.d, self.p + 4 * k)[0] for k in range(n)]
                    self.p += 4 * n
                elif typ == 3:
                    value = [self.line() for _ in range(n)]
                elif typ == 4:
                    self.u32()
                elif typ in (5, 6, 7):
                    value = self.u32()
            parent = self.u32()
            self.symbols.append({"name": name, "type": typ, "flags": flags, "count": n, "value": value, "parent": parent})
        size = self.u32()
        self.code = self.d[self.p:self.p + size]

    def sym(self, i):
        if 0 <= i < len(self.symbols):
            s = self.symbols[i]
            return s["name"] or "#%d" % i
        return "#%d" % i

    def functions(self):
        out = [(s["value"], i) for i, s in enumerate(self.symbols)
               if s["type"] in (5, 7) and isinstance(s["value"], int) and s["value"] > 0 and not (s["flags"] & 0x4)]
        return sorted(out)

    def disasm(self, name):
        name = name.upper()
        idx = next((i for i, s in enumerate(self.symbols) if s["name"].upper() == name and s["type"] == 5), None)
        if idx is None:
            print("no function", name)
            return
        start = self.symbols[idx]["value"]
        starts = [a for a, _ in self.functions() if a > start]
        end = starts[0] if starts else len(self.code)
        print("== %s (#%d) at %d..%d" % (self.symbols[idx]["name"], idx, start, end))
        pc = start
        while pc < end:
            op = self.code[pc]
            name_op = OPS.get(op, "op%d" % op)
            line = "%6d  %-8s" % (pc, name_op)
            if op in (61, 75, 76):
                line += " -> %d" % struct.unpack_from("<I", self.code, pc + 1)[0]
                size = 5
            elif op == 64:
                line += " %d" % struct.unpack_from("<i", self.code, pc + 1)[0]
                size = 5
            elif op in (62, 65, 67, 80):
                sy = struct.unpack_from("<I", self.code, pc + 1)[0]
                line += " " + self.sym(sy)
                size = 5
            elif op == 245:
                sy = struct.unpack_from("<I", self.code, pc + 1)[0]
                line += " %s[%d]" % (self.sym(sy), self.code[pc + 5])
                size = 6
            else:
                size = 1
            print(line)
            pc += size


if __name__ == "__main__":
    dat = Dat(sys.argv[1])
    for fn in sys.argv[2:]:
        dat.disasm(fn)
