"""The test register as a spreadsheet (owner, 2026-11-08: "instead of having ephemeral test matrices let's have an
excel file where we keep all the tests, passed and open so that I can just look at them any time and filter to only
show the open ones"; then "make it so the status cells are dropdowns where I can select the status").

docs/tests.csv is the register, one test per row:
  id, status, issue, feature, version, setup, do, expect, note, added, result_date, stream, source
status is one of Open, Passed, Failed, Superseded, Dropped. This script writes TESTS.xlsx at the checkout's root
(git-ignored): one sheet, newest issue first, the header frozen, a filter on every column (it opens showing the Open
tests; clear the Status filter to see them all). Each Status cell is a dropdown of the five, coloured by its value.

The owner may change a Status (and a Note) in TESTS.xlsx: every run first reads the saved sheet back and takes each
changed Status / Note into the register (a status change dated today), against TESTS.xlsx.base.json - what the last
run wrote. Standard library only.

  python -I scripts/tests_sheet.py                      the owner's edits taken in, then the sheet
  python -I scripts/tests_sheet.py set <#issue> <id> <status> [note]   a result (today's date), then the sheet
  python -I scripts/tests_sheet.py add <rows.csv>       rows appended (same columns), then the sheet
  python -I scripts/tests_sheet.py <register.csv> <out.xlsx>   the same on other files (for checks)
Edit the register only through these (or the csv module with newline=''): a cell may hold line breaks.
"""
import csv
import datetime
import io
import json
import os
import re
import sys
import zipfile
from xml.etree import ElementTree
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIELDS = ["id", "status", "issue", "feature", "version", "setup", "do", "expect", "note", "added", "result_date",
          "stream", "source"]
HEADERS = ["ID", "Status", "Issue", "Feature", "Version", "Setup", "Do", "Expect", "Note", "Added", "Result date",
           "Stream", "Source"]
WIDTHS = [9, 12, 7, 34, 8, 32, 42, 52, 34, 11, 11, 10, 12]
STATUSES = ["Open", "Passed", "Failed", "Superseded", "Dropped"]
STATUS_FILL = {"Open": "FFFFF2CC", "Passed": "FFE2EFDA", "Failed": "FFF8CBAD", "Superseded": "FFEDEDED",
               "Dropped": "FFEDEDED"}
SHOWN = "Open"  # the filter the sheet opens with
NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
      "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
      "p": "http://schemas.openxmlformats.org/package/2006/relationships"}


def read_register(path):
    with open(path, encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        assert reader.fieldnames == FIELDS, f"{path}: columns {reader.fieldnames}, want {FIELDS}"
        rows = list(reader)
    seen = set()
    for n, row in enumerate(rows, start=2):
        assert row["status"] in STATUSES, f"{path} line {n}: status {row['status']!r}"
        assert row["id"] and re.match(r"^#\d+$", row["issue"]), f"{path} line {n}: id / issue"
        key = (row["issue"], row["id"])
        assert key not in seen, f"{path} line {n}: {key} twice"
        seen.add(key)
    return rows


def write_register(path, rows):
    buf = io.StringIO()
    writer = csv.DictWriter(buf, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow(row)
    with open(path, "wb") as f:
        f.write(buf.getvalue().encode("utf-8"))


# --- reading a saved sheet back (as Excel or another program wrote it: shared or inline strings)

def column_index(ref):
    letters = re.match(r"[A-Z]+", ref).group(0)
    n = 0
    for ch in letters:
        n = n * 26 + ord(ch) - 64
    return n


def read_sheet(path):
    """The sheet's rows as dicts by header (row 1)."""
    with zipfile.ZipFile(path) as z:
        names = set(z.namelist())
        workbook = ElementTree.fromstring(z.read("xl/workbook.xml"))
        first = workbook.find("m:sheets/m:sheet", NS)
        rid = first.get(f"{{{NS['r']}}}id")
        rels = ElementTree.fromstring(z.read("xl/_rels/workbook.xml.rels"))
        target = next(r.get("Target") for r in rels.findall("p:Relationship", NS) if r.get("Id") == rid)
        sheet_name = target.lstrip("/") if target.startswith("/") else "xl/" + target
        shared = []
        if "xl/sharedStrings.xml" in names:
            for si in ElementTree.fromstring(z.read("xl/sharedStrings.xml")).findall("m:si", NS):
                shared.append("".join(t.text or "" for t in si.iter(f"{{{NS['m']}}}t")))
        sheet = ElementTree.fromstring(z.read(sheet_name))
    rows = []
    for row in sheet.findall("m:sheetData/m:row", NS):
        values = {}
        for c in row.findall("m:c", NS):
            kind = c.get("t")
            if kind == "inlineStr":
                value = "".join(t.text or "" for t in c.iter(f"{{{NS['m']}}}t"))
            else:
                v = c.find("m:v", NS)
                value = "" if v is None or v.text is None else v.text
                if kind == "s" and value:
                    value = shared[int(value)]
            values[column_index(c.get("r"))] = value
        rows.append(values)
    if not rows:
        return []
    header = {i: name for i, name in rows[0].items()}
    return [{header[i]: value for i, value in r.items() if i in header} for r in rows[1:]]


def import_edits(register, sheet_path, base_path):
    """The owner's Status / Note changes saved in the sheet, taken into the register. Returns the changes."""
    if not (os.path.exists(sheet_path) and os.path.exists(base_path)):
        return []
    try:
        sheet = read_sheet(sheet_path)
    except (OSError, KeyError, StopIteration, zipfile.BadZipFile, ElementTree.ParseError) as e:
        print(f"note: {sheet_path} not read back ({e}) - the owner's edits in it, if any, are not taken in")
        return []
    with open(base_path, encoding="utf-8") as f:
        base = json.load(f)
    rows = read_register(register)
    by_key = {(r["issue"], r["id"]): r for r in rows}
    today = datetime.date.today().isoformat()
    changes = []
    for s in sheet:
        key = (s.get("Issue", "").strip(), s.get("ID", "").strip())
        was = base.get("|".join(key))
        row = by_key.get(key)
        if was is None or row is None:
            continue
        status = s.get("Status", "").strip()
        if status != was["status"]:
            if status not in STATUSES:
                print(f"note: {key[0]} {key[1]}: status {status!r} in the sheet isn't one of {STATUSES} - ignored")
            else:
                row["status"] = status
                row["result_date"] = "" if status == "Open" else today
                changes.append(f"{key[0]} {key[1]}: {was['status']} -> {status} (set in the sheet)")
        note = s.get("Note", "")
        if note != was["note"]:
            row["note"] = note
            changes.append(f"{key[0]} {key[1]}: note set in the sheet")
    if changes:
        write_register(register, rows)
        read_register(register)
    return changes


# --- writing the sheet

def column(n):
    name = ""
    while n:
        n, r = divmod(n - 1, 26)
        name = chr(65 + r) + name
    return name


def text(value):
    value = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", value)
    return escape(value, {'"': "&quot;"})


def cell(ref, value, style):
    return (f'<c r="{ref}" s="{style}" t="inlineStr"><is><t xml:space="preserve">{text(value)}</t></is></c>'
            if value else f'<c r="{ref}" s="{style}"/>')


def sheet_xml(rows):
    last = column(len(FIELDS))
    end = len(rows) + 1
    out = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
           '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
           'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">',
           f'<dimension ref="A1:{last}{end}"/>',
           '<sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" '
           'state="frozen"/><selection pane="bottomLeft" activeCell="A2" sqref="A2"/></sheetView></sheetViews>',
           '<sheetFormatPr defaultRowHeight="15"/>', '<cols>']
    for i, width in enumerate(WIDTHS, start=1):
        out.append(f'<col min="{i}" max="{i}" width="{width}" customWidth="1"/>')
    out.append('</cols><sheetData>')
    out.append('<row r="1">' + "".join(cell(f"{column(i)}1", h, 1) for i, h in enumerate(HEADERS, start=1)) +
               '</row>')
    for r, row in enumerate(rows, start=2):
        hidden = ' hidden="1"' if row["status"] != SHOWN else ""
        cells = []
        for i, field in enumerate(FIELDS, start=1):
            ref = f"{column(i)}{r}"
            value = row[field]
            if field == "status":
                cells.append(cell(ref, value, 3))
            elif field == "source" and value.startswith("https://"):
                label = row["issue"] + (" comment" if "#issuecomment" in value else "")
                cells.append(f'<c r="{ref}" s="4" t="str"><f>HYPERLINK("{text(value)}","{text(label)}")</f>'
                             f'<v>{text(label)}</v></c>')
            else:
                cells.append(cell(ref, value, 2))
        out.append(f'<row r="{r}"{hidden}>' + "".join(cells) + '</row>')
    out.append('</sheetData>')
    out.append(f'<autoFilter ref="A1:{last}{end}"><filterColumn colId="1"><filters>'
               f'<filter val="{SHOWN}"/></filters></filterColumn></autoFilter>')
    # the Status column: coloured by its value, a dropdown of the statuses
    out.append(f'<conditionalFormatting sqref="B2:B{end}">')
    dxf = {status: n for n, status in enumerate(STATUS_FILL)}
    for priority, status in enumerate(STATUSES, start=1):
        out.append(f'<cfRule type="cellIs" dxfId="{dxf[status]}" priority="{priority}" operator="equal">'
                   f'<formula>{text(chr(34) + status + chr(34))}</formula></cfRule>')
    out.append('</conditionalFormatting>')
    out.append(f'<dataValidations count="1"><dataValidation type="list" allowBlank="0" showInputMessage="1" '
               f'showErrorMessage="1" errorTitle="Status" error="Pick one of the list." sqref="B2:B{end}">'
               f'<formula1>{text(chr(34) + ",".join(STATUSES) + chr(34))}</formula1></dataValidation>'
               f'</dataValidations>')
    out.append('<pageMargins left="0.7" right="0.7" top="0.75" bottom="0.75" header="0.3" footer="0.3"/>')
    out.append('</worksheet>')
    return "".join(out)


def styles_xml():
    dxfs = "".join(f'<dxf><fill><patternFill patternType="solid"><fgColor rgb="{rgb}"/><bgColor rgb="{rgb}"/>'
                   f'</patternFill></fill></dxf>' for rgb in STATUS_FILL.values())
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
        '<fonts count="3"><font><sz val="11"/><name val="Calibri"/><family val="2"/></font>'
        '<font><b/><sz val="11"/><name val="Calibri"/><family val="2"/></font>'
        '<font><u/><sz val="11"/><color rgb="FF0563C1"/><name val="Calibri"/><family val="2"/></font></fonts>'
        '<fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/>'
        '</fill><fill><patternFill patternType="solid"><fgColor rgb="FFD9E1F2"/><bgColor indexed="64"/></patternFill>'
        '</fill></fills>'
        '<borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders>'
        '<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>'
        '<cellXfs count="5">'
        '<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'
        '<xf numFmtId="0" fontId="1" fillId="2" borderId="0" xfId="0" applyFont="1" applyFill="1" applyAlignment="1">'
        '<alignment vertical="top" wrapText="1"/></xf>'
        '<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0" applyAlignment="1">'
        '<alignment vertical="top" wrapText="1"/></xf>'
        '<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0" applyAlignment="1">'
        '<alignment vertical="top"/></xf>'
        '<xf numFmtId="0" fontId="2" fillId="0" borderId="0" xfId="0" applyFont="1" applyAlignment="1">'
        '<alignment vertical="top"/></xf>'
        '</cellXfs>'
        '<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>'
        f'<dxfs count="{len(STATUS_FILL)}">{dxfs}</dxfs>'
        '</styleSheet>')


def workbook_xml(count):
    last = column(len(FIELDS))
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
            'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
            '<sheets><sheet name="Tests" sheetId="1" r:id="rId1"/></sheets>'
            f'<definedNames><definedName name="_xlnm._FilterDatabase" localSheetId="0" hidden="1">'
            f"Tests!$A$1:${last}${count + 1}</definedName></definedNames>"
            '<calcPr calcId="0" fullCalcOnLoad="1"/></workbook>')


PARTS = {
    "[Content_Types].xml":
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/xl/workbook.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
        '<Override PartName="/xl/worksheets/sheet1.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
        '<Override PartName="/xl/styles.xml" '
        'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
        '</Types>',
    "_rels/.rels":
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" '
        'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" '
        'Target="xl/workbook.xml"/></Relationships>',
    "xl/_rels/workbook.xml.rels":
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" '
        'Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" '
        'Target="worksheets/sheet1.xml"/>'
        '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" '
        'Target="styles.xml"/></Relationships>',
}


def write_sheet(rows, out, base_path):
    rows = sorted(rows, key=lambda row: -int(row["issue"][1:]))  # newest issue first; stable within an issue
    parts = dict(PARTS)
    parts["xl/workbook.xml"] = workbook_xml(len(rows))
    parts["xl/styles.xml"] = styles_xml()
    parts["xl/worksheets/sheet1.xml"] = sheet_xml(rows)
    for name, xml in parts.items():
        ElementTree.fromstring(xml.encode("utf-8"))  # every part well-formed
    temp = out + ".tmp"
    with zipfile.ZipFile(temp, "w", zipfile.ZIP_DEFLATED) as z:
        for name in ("[Content_Types].xml", "_rels/.rels", "xl/workbook.xml", "xl/_rels/workbook.xml.rels",
                     "xl/styles.xml", "xl/worksheets/sheet1.xml"):
            z.writestr(name, parts[name])
    try:
        os.replace(temp, out)
    except PermissionError:
        os.remove(temp)
        sys.exit(f"{out} is open: close it and run this again (the register itself is up to date)")
    base = {f"{r['issue']}|{r['id']}": {"status": r["status"], "note": r["note"]} for r in rows}
    with open(base_path, "w", encoding="utf-8") as f:
        json.dump(base, f, ensure_ascii=False, indent=0)
    counts = {}
    for row in rows:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
    print(f"wrote {out}: {len(rows)} tests", ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))


def edit(register, args):
    rows = read_register(register)
    if args[0] == "set" and len(args) in (4, 5):
        issue, rid, status = args[1], args[2], args[3]
        assert status in STATUSES, f"status {status!r}: one of {STATUSES}"
        hits = [row for row in rows if row["issue"] == issue and row["id"] == rid]
        assert len(hits) == 1, f"{issue} {rid}: {len(hits)} rows"
        hits[0]["status"] = status
        hits[0]["result_date"] = "" if status == "Open" else datetime.date.today().isoformat()
        if len(args) == 5:
            hits[0]["note"] = args[4]
        print(f"{issue} {rid}: {status}")
    elif args[0] == "add" and len(args) == 2:
        added = read_register(args[1])
        rows += added
        print(f"{len(added)} rows added")
    else:
        sys.exit(__doc__)
    write_register(register, rows)
    read_register(register)  # still valid (no row twice)


def main():
    register = os.path.join(ROOT, "docs", "tests.csv")
    out = os.path.join(ROOT, "TESTS.xlsx")
    command = None
    if len(sys.argv) > 1 and sys.argv[1] in ("set", "add"):
        command = sys.argv[1:]
    elif len(sys.argv) == 3:
        register, out = sys.argv[1], sys.argv[2]
    elif len(sys.argv) != 1:
        sys.exit(__doc__)
    base_path = out + ".base.json"
    for change in import_edits(register, out, base_path):
        print("from the sheet:", change)
    if command:
        edit(register, command)
    write_sheet(read_register(register), out, base_path)


if __name__ == "__main__":
    main()
