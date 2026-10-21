# GFF V3.2 <-> JSON (round-trip) for NWN .gui layouts (and any GFF).
# usage:
#   python -I gffjson.py dump  <file.gui> [out.json]     GFF -> JSON (stdout if no out)
#   python -I gffjson.py build <file.json> <out.gui>      JSON -> GFF
#   python -I gffjson.py check <file.gui> ...             dump+build, compare semantically (and bytes)
#   python -I gffjson.py tree  <file.gui> ...             one line per object (tag, type, model, parent hook, x/y/z, caption)
# JSON shape: {"file_type": "GUI ", "file_version": "V3.2", "root": STRUCT}
#   STRUCT = {"__struct_id": n, "Label": [type, value], ...}  (field order kept)
#   types: 0 BYTE 1 CHAR 2 WORD 3 SHORT 4 DWORD 5 INT 6 DWORD64 7 INT64 8 FLOAT 9 DOUBLE
#          10 CExoString 11 CResRef 12 CExoLocString {"strref": n, "strings": {"0": "..."}}
#          13 VOID (hex) 14 Struct (STRUCT) 15 List ([STRUCT,...]) 16 Orientation 17 Vector
import json, struct, sys

def read_gff(data):
    ft, fv = data[0:4].decode('latin1'), data[4:8].decode('latin1')
    so, sc, fo, fc, lo, lc, fdo, fdc, fio, fic, lio, lic = struct.unpack_from('<12I', data, 8)
    labels = [data[lo + i * 16: lo + i * 16 + 16].rstrip(b'\0').decode('latin1') for i in range(lc)]
    def field(i):
        t, li, v = struct.unpack_from('<3I', data, fo + i * 12)
        name = labels[li]
        if t in (0, 2, 4): val = v
        elif t == 1: val = struct.unpack('<b', struct.pack('<B', v & 0xff))[0]
        elif t == 3: val = struct.unpack('<h', struct.pack('<H', v & 0xffff))[0]
        elif t == 5: val = struct.unpack('<i', struct.pack('<I', v))[0]
        elif t == 8: val = struct.unpack('<f', struct.pack('<I', v))[0]
        elif t == 6: val = struct.unpack_from('<Q', data, fdo + v)[0]
        elif t == 7: val = struct.unpack_from('<q', data, fdo + v)[0]
        elif t == 9: val = struct.unpack_from('<d', data, fdo + v)[0]
        elif t == 10:
            n = struct.unpack_from('<I', data, fdo + v)[0]; val = data[fdo + v + 4: fdo + v + 4 + n].decode('latin1')
        elif t == 11:
            n = data[fdo + v]; val = data[fdo + v + 1: fdo + v + 1 + n].decode('latin1')
        elif t == 12:
            tot, strref, cnt = struct.unpack_from('<3I', data, fdo + v)
            p = fdo + v + 12; strings = {}
            for _ in range(cnt):
                sid, ln = struct.unpack_from('<2I', data, p); strings[str(sid)] = data[p + 8: p + 8 + ln].decode('latin1'); p += 8 + ln
            val = {"strref": strref if strref != 0xffffffff else -1, "strings": strings}
        elif t == 13:
            n = struct.unpack_from('<I', data, fdo + v)[0]; val = data[fdo + v + 4: fdo + v + 4 + n].hex()
        elif t == 14: val = struc(v)
        elif t == 15:
            n = struct.unpack_from('<I', data, lio + v)[0]
            val = [struc(struct.unpack_from('<I', data, lio + v + 4 + 4 * k)[0]) for k in range(n)]
        elif t == 16: val = list(struct.unpack_from('<4f', data, fdo + v))
        elif t == 17: val = list(struct.unpack_from('<3f', data, fdo + v))
        else: raise ValueError('field type %d' % t)
        return name, [t, val]
    def struc(i):
        sid, dof, fcnt = struct.unpack_from('<3I', data, so + i * 12)
        idx = [dof] if fcnt == 1 else [struct.unpack_from('<I', data, fio + dof + 4 * k)[0] for k in range(fcnt)]
        out = {"__struct_id": sid}
        for f in idx:
            n, tv = field(f); out[n] = tv
        return out
    return {"file_type": ft, "file_version": fv, "root": struc(0)}

def write_gff(doc):
    structs, fields, labels, fdata, findices, lindices = [], [], [], bytearray(), bytearray(), bytearray()
    lmap = {}
    def label(n):
        if n not in lmap: lmap[n] = len(labels); labels.append(n)
        return lmap[n]
    def add_struct(s):
        si = len(structs); structs.append(None)
        names = [k for k in s if k != '__struct_id']
        fidx = [add_field(k, s[k]) for k in names]
        if len(fidx) == 1: dof = fidx[0]
        else:
            dof = len(findices)
            for f in fidx: findices.extend(struct.pack('<I', f))
        structs[si] = (s.get('__struct_id', 0), dof if fidx else 0xffffffff if False else dof, len(fidx))
        return si
    def add_field(name, tv):
        t, v = tv
        fi = len(fields); fields.append(None)
        if t in (0, 2, 4): raw = v & 0xffffffff
        elif t == 1: raw = v & 0xff
        elif t == 3: raw = v & 0xffff
        elif t == 5: raw = struct.unpack('<I', struct.pack('<i', v))[0]
        elif t == 8: raw = struct.unpack('<I', struct.pack('<f', v))[0]
        elif t in (6, 7, 9, 10, 11, 12, 13, 16, 17):
            raw = len(fdata)
            if t == 6: fdata.extend(struct.pack('<Q', v))
            elif t == 7: fdata.extend(struct.pack('<q', v))
            elif t == 9: fdata.extend(struct.pack('<d', v))
            elif t == 10: e = v.encode('latin1'); fdata.extend(struct.pack('<I', len(e)) + e)
            elif t == 11: e = v.encode('latin1'); fdata.extend(bytes([len(e)]) + e)
            elif t == 12:
                body = bytearray(struct.pack('<2I', v['strref'] & 0xffffffff, len(v['strings'])))
                for sid, s in v['strings'].items():
                    e = s.encode('latin1'); body.extend(struct.pack('<2I', int(sid), len(e)) + e)
                fdata.extend(struct.pack('<I', len(body)) + body)
            elif t == 13: e = bytes.fromhex(v); fdata.extend(struct.pack('<I', len(e)) + e)
            elif t == 16: fdata.extend(struct.pack('<4f', *v))
            elif t == 17: fdata.extend(struct.pack('<3f', *v))
        elif t == 14: raw = add_struct(v)
        elif t == 15:
            raw = len(lindices)
            lindices.extend(struct.pack('<I', len(v))); slot = len(lindices); lindices.extend(b'\0' * 4 * len(v))
            for k, s in enumerate(v):
                si = add_struct(s); struct.pack_into('<I', lindices, slot + 4 * k, si)
        else: raise ValueError('field type %d' % t)
        fields[fi] = (t, label(name), raw)
        return fi
    add_struct(doc['root'])
    hdr = 56
    so = hdr; fo = so + 12 * len(structs); lo = fo + 12 * len(fields); fdo = lo + 16 * len(labels)
    fio = fdo + len(fdata); lio = fio + len(findices)
    out = bytearray(doc.get('file_type', 'GUI ').encode('latin1')[:4] + doc.get('file_version', 'V3.2').encode('latin1')[:4])
    out.extend(struct.pack('<12I', so, len(structs), fo, len(fields), lo, len(labels), fdo, len(fdata), fio, len(findices), lio, len(lindices)))
    for sid, dof, cnt in structs: out.extend(struct.pack('<3I', sid, dof, cnt))
    for t, li, raw in fields: out.extend(struct.pack('<3I', t, li, raw))
    for n in labels: out.extend(n.encode('latin1')[:16].ljust(16, b'\0'))
    out.extend(fdata); out.extend(findices); out.extend(lindices)
    return bytes(out)

def tree(doc):
    def cap(s):
        c = s.get('Obj_Caption')
        if not c: return ''
        c = c[1]; r = c.get('Obj_StrRef', [0, None])[1]
        txt = c.get('AurString_Text', [0, ''])[1]
        return ' caption(strref=%s%s font=%s)' % (r if r != 0xffffffff else -1, (' text=%r' % txt) if txt else '', c.get('AurString_Font', [0, ''])[1])
    def walk(s, d):
        g = lambda k: s.get(k, [0, ''])[1]
        print('%s%s type=%s model=%s parent=%s/%s xyz=%s,%s,%s%s' % ('  ' * d, g('Obj_Tag'), g('Obj_Type'), g('Obj_ResRef'), g('Obj_Parent'), g('Obj_Parent_Hook'),
              g('Obj_X'), g('Obj_Y'), g('Obj_Z'), cap(s)))
        for ch in s.get('Obj_ChildList', [0, []])[1]: walk(ch, d + 1)
    walk(doc['root'], 0)

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'dump':
        doc = read_gff(open(sys.argv[2], 'rb').read())
        s = json.dumps(doc, indent=1)
        if len(sys.argv) > 3: open(sys.argv[3], 'w', encoding='utf-8').write(s)
        else: print(s)
    elif cmd == 'build':
        open(sys.argv[3], 'wb').write(write_gff(json.load(open(sys.argv[2], encoding='utf-8'))))
    elif cmd == 'check':
        for f in sys.argv[2:]:
            a = open(f, 'rb').read(); doc = read_gff(a); b2 = write_gff(doc)
            same = read_gff(b2) == doc
            print('%s: %d bytes -> %d bytes, semantic round-trip %s, byte-identical %s' % (f, len(a), len(b2), 'OK' if same else 'FAIL', a == b2))
    elif cmd == 'tree':
        for f in sys.argv[2:]:
            print('== ' + f); tree(read_gff(open(f, 'rb').read()))
