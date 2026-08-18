import argparse
import re
import shutil
from pathlib import Path


SOURCE_EXTENSIONS = {
    ".cpp",
    ".h",
    ".hpp",
    ".cxx",
    ".cc",
}


# ============================================================
# Value token pattern
#
# Matches a single read target, which may be:
#   - Simple identifier:          a, m_user_idx, szName
#   - Member access:              obj.member, obj.member.sub
#   - Pointer member access:      ptr->member, ptr->member->sub
#   - Array subscript:            arr[i], arr[i+1]
#   - Combinations:               obj.arr[i].member, arr[i].m_field
#
# Does NOT match function calls or casts:
#   - foo.GetValue()
#   - (DWORD)value
# ============================================================

VALUE_TOKEN_STR = r"""
    [A-Za-z_][A-Za-z0-9_]*
    (?:
        \.[A-Za-z_][A-Za-z0-9_]*
        |->\s*[A-Za-z_][A-Za-z0-9_]*
        |\[[^\]]+\]
    )*
"""

VALUE_TOKEN_PATTERN = re.compile(VALUE_TOKEN_STR, re.VERBOSE)

# ============================================================
# Packet chain pattern
#
# Match:
#
#     rkPacket >> a >> b;
#     rkPacket >> a >> b >> c;
#     packet   >> a >> b >> c >> d;
#     rkPacket >> obj.member >> obj2.member2;
#     rkPacket >> arr[i] >> obj.arr[i].member;
#     rkPacket >> ptr->member >> ptr2->member2;
#     rkPacket >> a >> b
#           >> c >> d;                   (multi-line)
#
# Does NOT match:
#
#     rkPacket >> foo.GetValue() >> bar;
#     rkPacket >> (DWORD)value >> bar;
#     rkPacket >> a; // trailing comment
#
# Function calls and casts are intentionally excluded.
# Lines with trailing comments after ';' are skipped to
# avoid losing the comment during migration.
# ============================================================

CHAIN_PATTERN = re.compile(
    VALUE_TOKEN_STR.join([
        r"""
        ^
        (?P<indent>[ \t]*)
        (?P<packet>[A-Za-z_][A-Za-z0-9_]*)
        \s*>>\s*
        (?P<values>
        """,
        r"""
            (?:
                \s*>>\s*
        """,
        r"""
            )*
        )
        \s*;[ \t]*(?:\r?\n|\Z)
        """,
    ]),
    re.VERBOSE | re.MULTILINE,
)

# ============================================================
# Function detection pattern
#
# Matches function definitions that have SP2Packet in their
# parameter list. These are the packet handler functions
# that may contain >> chains.
#
# Example:
#     void User::OnItemCompound( SP2Packet &rkPacket )
#     bool User::OnExpandMedalSlotOpen( SP2Packet &rkPacket )
# ============================================================

FUNC_PATTERN = re.compile(
    r"""
    ^
    (?P<ret>\w+)\s+
    (?P<cls>\w+)::
    (?P<name>\w+)
    \s*\([^)]*SP2Packet[^)]*\)
    """,
    re.VERBOSE | re.MULTILINE,
)

# ============================================================
# Return type -> guard variant mapping
#
# Maps the function's return type to the appropriate
# PACKET_GUARD_*_READ variant.
#
# 'BREAK' is special: it's determined by switch context,
# not by return type. It takes priority over return type.
# ============================================================

GUARD_TYPE_MAP = {
    'void':   'VOID',
    'bool':   'bool',
    'BOOL':   'BOOL',
    'int':    'INT',
    'DWORD':  'INT',
    'LONG':   'INT',
    'UINT':   'INT',
    'BYTE':   'INT',
    'WORD':   'INT',
    'short':  'INT',
    'LPVOID': 'NULL',
    'LPTSTR': 'NULL',
    'HANDLE': 'NULL',
    'char':   'NULL',
    'DWORD_PTR': 'NULL',
    'INT_PTR':   'NULL',
}

DEFAULT_GUARD = 'VOID'


# ============================================================
# Encoding detection
# ============================================================

def detect_encoding(data: bytes):
    if data.startswith(b"\xef\xbb\xbf"):
        return "utf-8", b"\xef\xbb\xbf"
    if data.startswith(b"\xff\xfe\x00\x00"):
        return "utf-32-le", b"\xff\xfe\x00\x00"
    if data.startswith(b"\x00\x00\xfe\xff"):
        return "utf-32-be", b"\x00\x00\xfe\xff"
    if data.startswith(b"\xff\xfe"):
        return "utf-16-le", b"\xff\xfe"
    if data.startswith(b"\xfe\xff"):
        return "utf-16-be", b"\xfe\xff"

    try:
        data.decode("utf-8")
        return "utf-8", b""
    except UnicodeDecodeError:
        pass

    try:
        data.decode("cp949")
        return "cp949", b""
    except UnicodeDecodeError:
        pass

    try:
        data.decode("cp1252")
        return "cp1252", b""
    except UnicodeDecodeError:
        pass

    raise UnicodeDecodeError(
        "unknown", data, 0, len(data),
        "Unable to detect source encoding"
    )


def decode_file(path: Path, forced_encoding=None):
    data = path.read_bytes()

    if data.startswith(b"\xef\xbb\xbf"):
        return data[3:].decode("utf-8"), "utf-8", b"\xef\xbb\xbf"
    if data.startswith(b"\xff\xfe\x00\x00"):
        return data[4:].decode("utf-32-le"), "utf-32-le", b"\xff\xfe\x00\x00"
    if data.startswith(b"\x00\x00\xfe\xff"):
        return data[4:].decode("utf-32-be"), "utf-32-be", b"\x00\x00\xfe\xff"
    if data.startswith(b"\xff\xfe"):
        return data[2:].decode("utf-16-le"), "utf-16-le", b"\xff\xfe"
    if data.startswith(b"\xfe\xff"):
        return data[2:].decode("utf-16-be"), "utf-16-be", b"\xfe\xff"

    if forced_encoding:
        return data.decode(forced_encoding), forced_encoding, b""

    for enc in ("utf-8", "cp949", "cp1252"):
        try:
            return data.decode(enc), enc, b""
        except UnicodeDecodeError:
            pass

    raise UnicodeDecodeError(
        "unknown", data, 0, len(data),
        "Unable to detect source encoding"
    )


def encode_file(text: str, encoding: str, bom: bytes):
    encoded = text.encode(encoding)
    if bom:
        return bom + encoded
    return encoded


# ============================================================
# Brace scanning helpers
#
# These functions scan source text while properly skipping
# string literals, char literals, and comments.
# ============================================================

def _skip_junk(text, i, end):
    """Skip comments and string/char literals starting at i.
    Returns the position after the skipped content, or i if
    nothing to skip."""
    c = text[i]
    nc = text[i + 1] if i + 1 < end else '\0'

    if c == '/' and nc == '/':
        j = i + 2
        while j < end and text[j] != '\n':
            j += 1
        return j
    if c == '/' and nc == '*':
        j = i + 2
        while j < end - 1:
            if text[j] == '*' and text[j + 1] == '/':
                return j + 2
            j += 1
        return end
    if c == '"':
        j = i + 1
        while j < end:
            if text[j] == '\\':
                j += 2
                continue
            if text[j] == '"':
                return j + 1
            j += 1
        return end
    if c == "'":
        j = i + 1
        while j < end:
            if text[j] == '\\':
                j += 2
                continue
            if text[j] == "'":
                return j + 1
            j += 1
        return end

    return i


def find_opening_brace(text, start, end=None):
    """Find the first '{' after position start, skipping whitespace
    and comments. Returns -1 if ';' is found first (declaration)."""
    if end is None:
        end = len(text)
    i = start
    while i < end:
        c = text[i]
        if c == '{':
            return i
        if c == ';':
            return -1
        if c in ' \t\r\n':
            i += 1
            continue
        j = _skip_junk(text, i, end)
        if j != i:
            i = j
            continue
        i += 1
    return -1


def find_matching_brace(text, brace_pos, end=None):
    """Find the matching '}' for a '{' at brace_pos.
    Returns the position of '}' or -1 if not found."""
    if end is None:
        end = len(text)
    depth = 0
    i = brace_pos
    while i < end:
        c = text[i]
        if c == '{':
            depth += 1
            i += 1
            continue
        if c == '}':
            depth -= 1
            if depth == 0:
                return i
            i += 1
            continue
        if c in ' \t\r\n':
            i += 1
            continue
        j = _skip_junk(text, i, end)
        if j != i:
            i = j
            continue
        i += 1
    return -1


# ============================================================
# Function pre-scan
#
# Scans the file for function definitions that have SP2Packet
# in their parameter list. For each function, determines:
#   - Body range (opening brace to matching close)
#   - Return type
#   - Switch body ranges (for BREAK guard detection)
# ============================================================

def find_switch_bodies(text, start, end):
    """Find all switch body brace ranges within [start, end).
    Returns a list of (brace_open_pos, brace_close_pos) tuples."""
    bodies = []
    depth = 0
    pending_switch = False
    switch_stack = []
    i = start

    while i < end:
        c = text[i]

        if c == '{':
            depth += 1
            if pending_switch:
                switch_stack.append((i, depth))
                pending_switch = False
            i += 1
            continue

        if c == '}':
            if switch_stack and depth == switch_stack[-1][1]:
                bp, bd = switch_stack.pop()
                bodies.append((bp, i))
            depth -= 1
            i += 1
            continue

        if c in ' \t\r\n':
            i += 1
            continue

        j = _skip_junk(text, i, end)
        if j != i:
            i = j
            continue

        if c == 's' and not pending_switch:
            prev = text[i - 1] if i > 0 else '\n'
            if not (prev.isalnum() or prev == '_'):
                after = text[i + 6] if i + 6 < end else '\n'
                if text[i:i + 6] == 'switch' and not (after.isalnum() or after == '_'):
                    j = i + 6
                    while j < end and text[j] in ' \t\r\n':
                        j += 1
                    if j < end and text[j] == '(':
                        pdepth = 0
                        while j < end:
                            if text[j] == '(':
                                pdepth += 1
                            elif text[j] == ')':
                                pdepth -= 1
                                if pdepth == 0:
                                    break
                            j += 1
                        if j < end:
                            j += 1
                            while j < end and text[j] in ' \t\r\n':
                                j += 1
                            if j < end and text[j] == '{':
                                pending_switch = True
                                i = j
                                continue
        i += 1

    return bodies


def find_functions(text):
    """Pre-scan all function definitions with SP2Packet parameter.
    Returns a sorted list of dicts:
        body_start:    pos of opening {
        body_end:      pos of matching }
        return_type:   str (e.g. 'void', 'bool', 'int')
        name:          function name
        switch_bodies: [(open_pos, close_pos), ...]
    """
    functions = []

    for m in FUNC_PATTERN.finditer(text):
        ret = m.group('ret')
        name = m.group('name')

        brace_pos = find_opening_brace(text, m.end())
        if brace_pos < 0:
            continue

        body_end = find_matching_brace(text, brace_pos)
        if body_end < 0:
            continue

        switch_bodies = find_switch_bodies(text, brace_pos, body_end)

        functions.append({
            'body_start': brace_pos,
            'body_end': body_end,
            'return_type': ret,
            'name': name,
            'switch_bodies': switch_bodies,
        })

    functions.sort(key=lambda f: f['body_start'])
    return functions


def determine_guard_type(match_start, functions, guard_override=None):
    """Determine the correct guard variant for a >> chain at match_start.
    Returns a string like 'VOID', 'bool', 'INT', 'BREAK', 'NULL', etc."""
    if guard_override and guard_override != 'auto':
        return guard_override

    func = None
    for f in functions:
        if f['body_start'] <= match_start < f['body_end']:
            func = f
            break

    if not func:
        return DEFAULT_GUARD

    for (sw_start, sw_end) in func['switch_bodies']:
        if sw_start <= match_start < sw_end:
            return 'BREAK'

    return GUARD_TYPE_MAP.get(func['return_type'], DEFAULT_GUARD)


# ============================================================
# Transformation
# ============================================================

def detect_line_ending(text):
    if '\r\n' in text:
        return '\r\n'
    return '\n'


def transform_text(text, guard_override=None):
    line_ending = detect_line_ending(text)
    functions = find_functions(text)

    changes = []
    result_parts = []
    last_end = 0

    for match in CHAIN_PATTERN.finditer(text):
        result_parts.append(text[last_end:match.start()])

        indent = match.group("indent")
        packet = match.group("packet")
        values_text = match.group("values")

        values = VALUE_TOKEN_PATTERN.findall(values_text)

        if not values:
            result_parts.append(match.group(0))
            last_end = match.end()
            continue

        guard = determine_guard_type(
            match.start(), functions, guard_override
        )
        macro_name = f"PACKET_GUARD_{guard}_READ"

        lines = [
            f"{indent}{macro_name}({packet}, {value});"
            for value in values
        ]
        replacement = line_ending.join(lines) + line_ending

        result_parts.append(replacement)

        line_number = text.count('\n', 0, match.start()) + 1

        changes.append({
            "line": line_number,
            "old": match.group(0),
            "new": replacement,
            "guard": guard,
        })

        last_end = match.end()

    result_parts.append(text[last_end:])

    return "".join(result_parts), changes


# ============================================================
# Display changes
# ============================================================

def show_changes(changes):
    for change in changes:
        line_number = change["line"]
        old = change["old"].rstrip("\r\n")
        new = change["new"].rstrip("\r\n")
        guard = change.get("guard", "")

        print(f"  [Line {line_number}] guard={guard}")

        for old_line in old.splitlines():
            print(f"  - {old_line}")

        for new_line in new.splitlines():
            print(f"  + {new_line}")

        print()


# ============================================================
# File processing
# ============================================================

def process_file(
    path: Path,
    dry_run=False,
    force_encoding=None,
    show_diff=False,
    guard_override=None,
):
    try:
        text, encoding, bom = decode_file(path, force_encoding)
    except Exception as e:
        print(f"[ERROR]  {path}")
        print(f"         {e}")
        print()
        return False

    new_text, changes = transform_text(text, guard_override)

    if not changes:
        print(f"[SKIP]   {path}")
        if encoding:
            print(f"         encoding={encoding}")
        return False

    guard_counts = {}
    for c in changes:
        g = c.get("guard", "?")
        guard_counts[g] = guard_counts.get(g, 0) + 1

    print(f"[CHANGE] {path}")
    print(f"         encoding={encoding}")
    print(f"         changes={len(changes)}")
    print(f"         guards={guard_counts}")

    if show_diff:
        print()
        show_changes(changes)

    if dry_run:
        print("         DRY RUN - file not modified")
        print()
        return True

    backup = Path(str(path) + ".bak")
    if not backup.exists():
        try:
            shutil.copy2(path, backup)
            print(f"         backup={backup}")
        except Exception as e:
            print(f"[ERROR]  Failed creating backup")
            print(f"         {e}")
            print()
            return False
    else:
        print(f"         backup already exists={backup}")

    try:
        new_data = encode_file(new_text, encoding, bom)
        path.write_bytes(new_data)
    except Exception as e:
        print(f"[ERROR]  Failed writing {path}")
        print(f"         {e}")
        print()
        return False

    print("         file modified")
    print()
    return True


# ============================================================
# File collection
# ============================================================

def collect_files(path: Path, recursive=False):
    if path.is_file():
        return [path]
    if not path.is_dir():
        return []

    iterator = path.rglob("*") if recursive else path.glob("*")

    files = []
    for p in iterator:
        if not p.is_file():
            continue
        if p.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        files.append(p)

    return files


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description=(
            "Migrate packet operator >> chains "
            "to PACKET_GUARD_*_READ() with auto-detection "
            "of function return type and switch context."
        )
    )

    parser.add_argument(
        "path",
        help="Source file or directory"
    )

    parser.add_argument(
        "-r", "--recursive",
        action="store_true",
        help="Process directories recursively"
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show changes without modifying files"
    )

    parser.add_argument(
        "--show-diff",
        action="store_true",
        help="Show detailed line-by-line changes"
    )

    parser.add_argument(
        "--encoding",
        help="Force encoding for files without BOM"
    )

    parser.add_argument(
        "--guard-type",
        default="auto",
        choices=["auto", "VOID", "bool", "BOOL", "INT",
                 "BREAK", "NULL"],
        help=(
            "Guard variant to use (default: auto-detect). "
            "Override when auto-detection is not desired."
        )
    )

    args = parser.parse_args()

    root = Path(args.path)
    if not root.exists():
        print(f"[ERROR] Path does not exist:")
        print(f"        {root}")
        return

    files = collect_files(root, args.recursive)
    if not files:
        print("No source files found.")
        return

    print(f"Files found: {len(files)}")
    print()

    changed = 0
    for path in files:
        if process_file(
            path,
            dry_run=args.dry_run,
            force_encoding=args.encoding,
            show_diff=args.show_diff,
            guard_override=args.guard_type,
        ):
            changed += 1

    print("----------------------------------------")
    print(f"Files found  : {len(files)}")
    print(f"Files changed: {changed}")


if __name__ == "__main__":
    main()