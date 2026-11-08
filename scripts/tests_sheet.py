"""The test register as a spreadsheet (owner, 2026-11-08: "instead of having ephemeral test matrices let's have an
excel file where we keep all the tests, passed and open so that I can just look at them any time and filter to only
show the open ones").

docs/tests.csv is the register, one test per row:
  id, status, issue, feature, version, setup, do, expect, note, added, result_date, stream, source
status is one of Open, Passed, Failed, Superseded, Dropped. This script writes TESTS.xlsx at the checkout's root
(git-ignored): one sheet, newest issue first, the header frozen, a filter on every column (it opens showing the Open
tests; clear the Status filter to see them all), the status coloured. Standard library only.

  python -I scripts/tests_sheet.py [register.csv] [out.xlsx]
"""
import csv
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
WIDTHS = [9, 11, 7, 34, 8, 32, 42, 52, 34, 11, 11, 10, 12]
STATUSES = {"Open": 3, "Passed": 4, "Failed": 5, "Superseded": 6, "Dropped": 6}  # -> cell style
SHOWN = "Open"  # the filter the sheet opens with


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
    out = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
           '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
           'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">',
           f'<dimension ref="A1:{last}{len(rows) + 1}"/>',
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
                cells.append(cell(ref, value, STATUSES[value]))
            elif field == "source" and value.startswith("https://"):
                label = row["issue"] + (" comment" if "#issuecomment" in value else "")
                cells.append(f'<c r="{ref}" s="7" t="str"><f>HYPERLINK("{text(value)}","{text(label)}")</f>'
                             f'<v>{text(label)}</v></c>')
            else:
                cells.append(cell(ref, value, 2))
        out.append(f'<row r="{r}"{hidden}>' + "".join(cells) + '</row>')
    out.append('</sheetData>')
    out.append(f'<autoFilter ref="A1:{last}{len(rows) + 1}"><filterColumn colId="1"><filters>'
               f'<filter val="{SHOWN}"/></filters></filterColumn></autoFilter>')
    out.append('<pageMargins left="0.7" right="0.7" top="0.75" bottom="0.75" header="0.3" footer="0.3"/>')
    out.append('</worksheet>')
    return "".join(out)


STYLES = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
    '<fonts count="3"><font><sz val="11"/><name val="Calibri"/><family val="2"/></font>'
    '<font><b/><sz val="11"/><name val="Calibri"/><family val="2"/></font>'
    '<font><u/><sz val="11"/><color rgb="FF0563C1"/><name val="Calibri"/><family val="2"/></font></fonts>'
    '<fills count="7"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill>'
    '<fill><patternFill patternType="solid"><fgColor rgb="FFD9E1F2"/><bgColor indexed="64"/></patternFill></fill>'
    '<fill><patternFill patternType="solid"><fgColor rgb="FFFFF2CC"/><bgColor indexed="64"/></patternFill></fill>'
    '<fill><patternFill patternType="solid"><fgColor rgb="FFE2EFDA"/><bgColor indexed="64"/></patternFill></fill>'
    '<fill><patternFill patternType="solid"><fgColor rgb="FFF8CBAD"/><bgColor indexed="64"/></patternFill></fill>'
    '<fill><patternFill patternType="solid"><fgColor rgb="FFEDEDED"/><bgColor indexed="64"/></patternFill></fill>'
    '</fills>'
    '<borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders>'
    '<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>'
    '<cellXfs count="8">'
    '<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'
    '<xf numFmtId="0" fontId="1" fillId="2" borderId="0" xfId="0" applyFont="1" applyFill="1" applyAlignment="1">'
    '<alignment vertical="top" wrapText="1"/></xf>'
    '<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0" applyAlignment="1">'
    '<alignment vertical="top" wrapText="1"/></xf>'
    '<xf numFmtId="0" fontId="0" fillId="3" borderId="0" xfId="0" applyFill="1" applyAlignment="1">'
    '<alignment vertical="top"/></xf>'
    '<xf numFmtId="0" fontId="0" fillId="4" borderId="0" xfId="0" applyFill="1" applyAlignment="1">'
    '<alignment vertical="top"/></xf>'
    '<xf numFmtId="0" fontId="0" fillId="5" borderId="0" xfId="0" applyFill="1" applyAlignment="1">'
    '<alignment vertical="top"/></xf>'
    '<xf numFmtId="0" fontId="0" fillId="6" borderId="0" xfId="0" applyFill="1" applyAlignment="1">'
    '<alignment vertical="top"/></xf>'
    '<xf numFmtId="0" fontId="2" fillId="0" borderId="0" xfId="0" applyFont="1" applyAlignment="1">'
    '<alignment vertical="top"/></xf>'
    '</cellXfs>'
    '<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>'
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


def main():
    register = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "docs", "tests.csv")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "TESTS.xlsx")
    rows = read_register(register)
    rows.sort(key=lambda row: -int(row["issue"][1:]))  # newest issue first; a stable sort keeps each issue's order
    parts = dict(PARTS)
    parts["xl/workbook.xml"] = workbook_xml(len(rows))
    parts["xl/styles.xml"] = STYLES
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
        sys.exit(f"{out} is open in Excel: close it and run this again")
    counts = {}
    for row in rows:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
    print(f"wrote {out}: {len(rows)} tests", ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))


if __name__ == "__main__":
    main()
