"""tests/data/fixtures/text_literals.txt: each src/core file's chat messages still written in the code - the same
count as tests/data/text_literals_test.cpp (keep the two alike). Usage: python -I literal_baseline.py <repo> <out>"""
import pathlib
import sys

root, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])


def literal_calls(text, name):
    count, opener = 0, name + "("
    at = text.find(opener)
    while at != -1:
        boundary = at == 0 or not (text[at - 1].isalnum() or text[at - 1] == "_")
        if boundary and not (at >= 5 and text[at - 5:at] == "void "):
            i, depth, in_string, args = at + len(opener), 1, False, []
            while i < len(text) and depth > 0:
                c = text[i]
                if in_string:
                    if c == "\\":
                        args.append(c)
                        i += 1
                        if i < len(text):
                            args.append(text[i])
                        i += 1
                        continue
                    if c == '"':
                        in_string = False
                elif c == '"':
                    in_string = True
                elif c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                args.append(c)
                i += 1
            a = "".join(args)
            if '"' in a and "PlayerText(" not in a:
                count += 1
        at = text.find(opener, at + 1)
    return count


def chat_line_literals(text):
    """String literals in code (not in a comment, nor escaped inside another string) whose text starts "nwn5e: "."""
    count, i, n = 0, 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            if j == -1:
                break
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            if j == -1:
                break
            i = j + 1
        elif c in "'\"":
            if c == '"' and text.startswith("nwn5e: ", i + 1):
                count += 1
            i += 1
            while i < n and text[i] != c:
                if text[i] == "\\":
                    i += 1
                i += 1
        i += 1
    return count


rows, total_t, total_h, total_s, total_l = [], 0, 0, 0, 0
for p in sorted((root / "src/core").glob("*.cpp")):
    text = p.read_text(encoding="utf-8", errors="replace")
    t, h, s = literal_calls(text, "TellPlayer"), literal_calls(text, "Tell"), text.count("SendMessageToPC(")
    lines = chat_line_literals(text)
    if t or h or s or lines:
        rows.append("%s %d %d %d %d" % (p.name, t, h, s, lines))
        total_t += t
        total_h += h
        total_s += s
        total_l += lines
out.write_text("# #168: each src/core file's chat messages still written in the code - <file> <TellPlayer literals>\n"
               "# <Tell helper literals> <SendMessageToPC calls> <literals starting \"nwn5e: \">.\n"
               "# tests/data/text_literals_test.cpp holds each at most this: lower a file's numbers when its\n"
               "# messages move to data/text/en.txt (scripts/text_literals.py).\n"
               + "\n".join(rows) + "\n", encoding="utf-8", newline="\n")
print(len(rows), "files,", total_t, "TellPlayer literals,", total_h, "Tell helper literals,", total_s, "SendMessageToPC,",
      total_l, "\"nwn5e: \" literals")
