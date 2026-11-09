"""The test board (#148; owner, 2026-11-08: "go with A"): every test an item of the GitHub Project "nwn5e tests"
(https://github.com/users/BrunoTCarreiro/projects/3), edited by the owner in the browser and by the sessions through gh
at the same time - no file to lock.

Columns: Status (Open, Passed, Failed, Superseded, Dropped), ID, Issue (a number), Version, Stream (game, animation),
Setup, Do, Expect, Note, Added, Result date, Source. A text column holds 1024 characters; the item's description holds
the whole test. docs/tests.csv is the archive (the tests passed, superseded or dropped before the board) and, after
`export`, a full record of both.

  python -I scripts/tests_board.py list [open|all]                  the board's tests
  python -I scripts/tests_board.py add <rows.csv>                   new tests (docs/tests.csv's columns) onto the board
  python -I scripts/tests_board.py set <#issue> <id> <status> [note]   a result, dated today ("Open" clears the date)
  python -I scripts/tests_board.py edit <#issue> <id> <column> <text>   a text column (Setup, Do, Expect, Note ...)
                                                                     changed, the item's title and description too
  python -I scripts/tests_board.py export                           docs/tests.csv = the archive + the board
  python -I scripts/tests_board.py move                             the register's Open and Failed rows not yet on it
"""
import csv
import datetime
import io
import json
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTER = os.path.join(ROOT, "docs", "tests.csv")
OWNER = "BrunoTCarreiro"
NUMBER = 3
FIELDS = ["id", "status", "issue", "feature", "version", "setup", "do", "expect", "note", "added", "result_date",
          "stream", "source"]
STATUSES = ["Open", "Passed", "Failed", "Superseded", "Dropped"]
TEXT_COLUMNS = {"id": "ID", "version": "Version", "setup": "Setup", "do": "Do", "expect": "Expect", "note": "Note",
                "source": "Source"}
DATE_COLUMNS = {"added": "Added", "result_date": "Result date"}
TEXT_LIMIT = 1024
CUT = " … (the whole text: the item's description)"
PAUSE = 0.8  # seconds between requests: GitHub's limit on content-creating requests


def gh(query, variables=None, tries=8):
    body = json.dumps({"query": query, "variables": variables or {}})
    for attempt in range(tries):
        r = subprocess.run(["gh", "api", "graphql", "--input", "-"], input=body, capture_output=True, text=True,
                           encoding="utf-8")
        out = r.stdout.strip()
        data = json.loads(out) if out.startswith("{") else {}
        errors = data.get("errors") or ([] if r.returncode == 0 else [{"message": r.stderr.strip()}])
        if not errors:
            time.sleep(PAUSE)
            return data["data"]
        text = json.dumps(errors)
        if "rate limit" in text.lower() or "abuse" in text.lower() or "was submitted too quickly" in text.lower():
            wait = 60 * (attempt + 1)
            print(f"  rate limited - waiting {wait} s", flush=True)
            time.sleep(wait)
            continue
        sys.exit(f"gh: {text[:600]}")
    sys.exit("gh: still rate limited")


def project():
    """The project's id and its fields: name -> (id, {option name: option id})."""
    data = gh("""query($o: String!, $n: Int!) { user(login: $o) { projectV2(number: $n) { id
                 fields(first: 50) { nodes {
                   ... on ProjectV2FieldCommon { id name }
                   ... on ProjectV2SingleSelectField { id name options { id name } } } } } } }""",
              {"o": OWNER, "n": NUMBER})
    p = data["user"]["projectV2"]
    fields = {}
    for f in p["fields"]["nodes"]:
        if f:
            fields[f["name"]] = (f["id"], {o["name"]: o["id"] for o in f.get("options", [])})
    for name in ["Status", "Stream", "Issue"] + list(TEXT_COLUMNS.values()) + list(DATE_COLUMNS.values()):
        assert name in fields, f"the board has no {name!r} column"
    for s in STATUSES:
        assert s in fields["Status"][1], f"Status has no {s!r}"
    return p["id"], fields


def items(pid):
    """Every item: {"item": id, "title", "body", field values by column name}."""
    out, after = [], None
    while True:
        data = gh("""query($id: ID!, $after: String) { node(id: $id) { ... on ProjectV2 {
                       items(first: 100, after: $after) { pageInfo { hasNextPage endCursor } nodes { id
                         content { ... on DraftIssue { id title body } }
                         fieldValues(first: 30) { nodes {
                           ... on ProjectV2ItemFieldTextValue { text field { ... on ProjectV2FieldCommon { name } } }
                           ... on ProjectV2ItemFieldNumberValue { number field { ... on ProjectV2FieldCommon { name } } }
                           ... on ProjectV2ItemFieldDateValue { date field { ... on ProjectV2FieldCommon { name } } }
                           ... on ProjectV2ItemFieldSingleSelectValue { name
                             field { ... on ProjectV2FieldCommon { name } } } } } } } } } }""",
                  {"id": pid, "after": after})
        page = data["node"]["items"]
        for node in page["nodes"]:
            row = {"item": node["id"], "draft": (node["content"] or {}).get("id", ""),
                   "title": (node["content"] or {}).get("title", ""),
                   "body": (node["content"] or {}).get("body", "")}
            for v in node["fieldValues"]["nodes"]:
                if not v or "field" not in v:
                    continue
                name = v["field"]["name"]
                row[name] = v.get("text", v.get("date", v.get("name", v.get("number"))))
            out.append(row)
        if not page["pageInfo"]["hasNextPage"]:
            return out
        after = page["pageInfo"]["endCursor"]


def read_rows(path):
    with open(path, encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        assert reader.fieldnames == FIELDS, f"{path}: columns {reader.fieldnames}, want {FIELDS}"
        rows = list(reader)
    for n, row in enumerate(rows, start=2):
        assert row["status"] in STATUSES, f"{path} line {n}: status {row['status']!r}"
        assert row["id"] and re.match(r"^#\d+$", row["issue"]), f"{path} line {n}: id / issue"
    return rows


def write_rows(path, rows):
    buf = io.StringIO()
    writer = csv.DictWriter(buf, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow(row)
    with open(path, "wb") as f:
        f.write(buf.getvalue().encode("utf-8"))


def fit(text):
    return text if len(text) <= TEXT_LIMIT else text[:TEXT_LIMIT - len(CUT)] + CUT


def body_of(row):
    parts = [f"**Issue:** {row['issue']} ({row['feature']}) · **Version:** {row['version'] or '-'} · "
             f"**Stream:** {row['stream']}"]
    for label, key in (("Setup", "setup"), ("Do", "do"), ("Expect", "expect"), ("Note", "note"),
                       ("Source", "source")):
        if row[key]:
            parts.append(f"**{label}:** {row[key]}")
    return "\n\n".join(parts)


def title_of(row):
    words = (row["do"] or row["expect"]).replace("\n", " ")
    if row["setup"]:
        words = row["setup"].replace("\n", " ") + " - " + words
    title = f"{row['id']}: {words}"
    return title if len(title) <= 120 else title[:117] + "..."


def set_fields(pid, fields, item, values):
    """values: column name -> value (str for text / date / select name, int for Issue; "" clears)."""
    decl, body, variables, n = ["$p: ID!", "$i: ID!"], [], {"p": pid, "i": item}, 0
    for name, value in values.items():
        fid, options = fields[name]
        n += 1
        if value in ("", None):
            body.append(f'c{n}: clearProjectV2ItemFieldValue(input: {{projectId: $p, itemId: $i, fieldId: "{fid}"}}) '
                        "{ clientMutationId }")
            continue
        if options:
            kind, typ, val = "singleSelectOptionId", "String!", options[value]
        elif name == "Issue":
            kind, typ, val = "number", "Float!", float(value)
        elif name in DATE_COLUMNS.values():
            kind, typ, val = "date", "Date!", value
        else:
            kind, typ, val = "text", "String!", fit(value)
        decl.append(f"$v{n}: {typ}")
        variables[f"v{n}"] = val
        body.append(f'u{n}: updateProjectV2ItemFieldValue(input: {{projectId: $p, itemId: $i, fieldId: "{fid}", '
                    f"value: {{{kind}: $v{n}}}}}) {{ clientMutationId }}")
    gh(f"mutation({', '.join(decl)}) {{ {' '.join(body)} }}", variables)


def add_rows(pid, fields, rows, batch=10):
    for start in range(0, len(rows), batch):
        chunk = rows[start:start + batch]
        decl, body, variables = ["$p: ID!"], [], {"p": pid}
        for k, row in enumerate(chunk):
            decl += [f"$t{k}: String!", f"$b{k}: String!"]
            variables[f"t{k}"], variables[f"b{k}"] = title_of(row), body_of(row)
            body.append(f"a{k}: addProjectV2DraftIssue(input: {{projectId: $p, title: $t{k}, body: $b{k}}}) "
                        "{ projectItem { id } }")
        made = gh(f"mutation({', '.join(decl)}) {{ {' '.join(body)} }}", variables)
        for k, row in enumerate(chunk):
            item = made[f"a{k}"]["projectItem"]["id"]
            values = {"Status": row["status"], "Stream": row["stream"] or "game", "Issue": int(row["issue"][1:])}
            for key, name in TEXT_COLUMNS.items():
                if row[key]:
                    values[name] = row[key]
            for key, name in DATE_COLUMNS.items():
                if row[key]:
                    values[name] = row[key]
            set_fields(pid, fields, item, values)
        print(f"  {min(start + batch, len(rows))} of {len(rows)} on the board", flush=True)


def key_of_item(it):
    return (f"#{int(it['Issue'])}" if it.get("Issue") is not None else "", it.get("ID", ""))


def section(body, label):
    m = re.search(rf"\*\*{label}:\*\* (.*?)(?=\n\n\*\*[A-Z][a-z]+:\*\* |\Z)", body or "", re.S)
    return m.group(1) if m else ""


def row_of_item(it, archive_row):
    row = dict(archive_row) if archive_row else {k: "" for k in FIELDS}
    row["id"], row["issue"] = key_of_item(it)[1], key_of_item(it)[0]
    row["status"] = it.get("Status", "Open")
    row["stream"] = it.get("Stream", row["stream"] or "game")
    for key, name in TEXT_COLUMNS.items():
        value = it.get(name, "") or ""
        if value.endswith(CUT):
            value = section(it["body"], name) or value
        row[key] = value
    for key, name in DATE_COLUMNS.items():
        row[key] = it.get(name, "") or ""
    if not row["feature"]:
        m = re.search(r"\*\*Issue:\*\* #\d+ \((.*?)\) ·", it.get("body", ""))
        row["feature"] = m.group(1) if m else ""
    return row


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    args = sys.argv[1:]
    if not args or args[0] not in ("list", "add", "set", "edit", "export", "move"):
        sys.exit(__doc__)
    pid, fields = project()
    if args[0] == "list":
        which = args[1] if len(args) > 1 else "open"
        board = items(pid)
        board.sort(key=lambda it: (-(it.get("Issue") or 0), it.get("ID", "")))
        for it in board:
            if which == "open" and it.get("Status") not in ("Open", "Failed"):
                continue
            issue, rid = key_of_item(it)
            print(f"{issue:6} {rid:8} {it.get('Status', ''):10} {it['title']}")
        print(f"{len(board)} tests on the board")
    elif args[0] in ("add", "move"):
        board = {key_of_item(it) for it in items(pid)}
        if args[0] == "add":
            assert len(args) == 2, __doc__
            rows = read_rows(args[1])
        else:
            rows = [r for r in read_rows(REGISTER) if r["status"] in ("Open", "Failed")]
        new = [r for r in rows if (r["issue"], r["id"]) not in board]
        for r in rows:
            if (r["issue"], r["id"]) in board:
                print(f"  already on the board: {r['issue']} {r['id']}")
        new.sort(key=lambda r: (-int(r["issue"][1:])))  # newest issue first, each issue in its own order
        add_rows(pid, fields, new)
        print(f"{len(new)} tests added")
    elif args[0] == "set":
        assert len(args) in (4, 5), __doc__
        issue, rid, status = args[1], args[2], args[3]
        assert status in STATUSES, f"status {status!r}: one of {STATUSES}"
        hits = [it for it in items(pid) if key_of_item(it) == (issue, rid)]
        assert len(hits) == 1, f"{issue} {rid}: {len(hits)} items on the board"
        values = {"Status": status, "Result date": "" if status == "Open" else datetime.date.today().isoformat()}
        if len(args) == 5:
            values["Note"] = args[4]
        set_fields(pid, fields, hits[0]["item"], values)
        print(f"{issue} {rid}: {status}")
    elif args[0] == "edit":
        assert len(args) == 5, __doc__
        issue, rid, column, text = args[1], args[2], args[3], args[4]
        key = next((k for k, name in TEXT_COLUMNS.items() if name.lower() == column.lower()), None)
        assert key and key != "id", f"column {column!r}: one of {list(TEXT_COLUMNS.values())}"
        hits = [it for it in items(pid) if key_of_item(it) == (issue, rid)]
        assert len(hits) == 1, f"{issue} {rid}: {len(hits)} items on the board"
        it = hits[0]
        set_fields(pid, fields, it["item"], {TEXT_COLUMNS[key]: text})
        row = row_of_item(it, None)
        row[key] = text
        gh("mutation($d: ID!, $t: String!, $b: String!) { updateProjectV2DraftIssue(input: "
           "{draftIssueId: $d, title: $t, body: $b}) { draftIssue { id } } }",
           {"d": it["draft"], "t": title_of(row), "b": body_of(row)})
        print(f"{issue} {rid}: {TEXT_COLUMNS[key]} set")
    elif args[0] == "export":
        archive = read_rows(REGISTER)
        by_key = {(r["issue"], r["id"]): r for r in archive}
        board = items(pid)
        seen = set()
        for it in board:
            key = key_of_item(it)
            seen.add(key)
            row = row_of_item(it, by_key.get(key))
            if key in by_key:
                by_key[key].update(row)
            else:
                archive.append(row)
                by_key[key] = row
        write_rows(REGISTER, archive)
        read_rows(REGISTER)
        print(f"wrote {REGISTER}: {len(archive)} tests, {len(seen)} from the board")


if __name__ == "__main__":
    main()
