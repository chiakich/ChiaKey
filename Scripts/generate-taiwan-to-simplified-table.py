#!/usr/bin/env python3
# Builds TaiwanToSimplifiedTable.inc from the vendored OpenCC tw2s dictionaries
# and ChiaKey's overrides. Run with --check to verify the checked-in table.

import argparse
import pathlib
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parent.parent
HAN = ROOT / "ChiaKey-Source" / "ModulePackages" / "OVOFHanConvert"
DATA = HAN / "OpenCC"
OVERRIDES = HAN / "TaiwanToSimplifiedOverrides.txt"
OUTPUT = HAN / "TaiwanToSimplifiedTable.inc"

REVERSE_PREFER = "# @reverse-prefer:"
OVERRIDABLE = ("TWVariantsRevPhrases", "TSPhrases", "TSCharacters")
# MSVC rejects string literals over 64 KiB; offsets are also stored as uint16_t.
POOL_LIMIT = 65535


def entries(path):
    with open(path, encoding="utf-8") as fh:
        for number, line in enumerate(fh, 1):
            line = line.rstrip("\r\n")
            if not line.strip() or line.startswith("#"):
                continue
            key, values = line.split("\t", 1)
            yield number, key, values.split(" ")


def read_dict(name):
    table = {}
    for _, key, values in entries(DATA / f"{name}.txt"):
        # OpenCC keeps the first occurrence and outputs the first value.
        table.setdefault(key, values[0])
    return table


def reverse_variants(name):
    # Mirrors OpenCC's data/scripts/common.py reverse_items.
    reverse, prefer = {}, {}
    with open(DATA / f"{name}.txt", encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            if line.startswith(REVERSE_PREFER):
                fields = line[len(REVERSE_PREFER):].split()
                prefer[fields[0]] = fields[-1]
    for _, key, values in entries(DATA / f"{name}.txt"):
        for value in values:
            reverse.setdefault(value, []).append(key)
    for key, value in prefer.items():
        if value not in reverse.get(key, []):
            raise SystemExit(f"{name}: reverse preference {key} -> {value} has no mapping")
        reverse[key].remove(value)
        reverse[key].insert(0, value)
    return {key: values[0] for key, values in reverse.items()}


def apply_overrides(tables, path, errors):
    added = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split("\t")
        where = f"{path.name}:{number}"
        if len(fields) != 3 or fields[0] not in OVERRIDABLE:
            errors.append(f"{where}: expected dictionary<TAB>key<TAB>value with one of {OVERRIDABLE}")
            continue
        name, key, value = fields
        table = tables[name]
        if name == "TSCharacters" and len(key) != 1:
            errors.append(f"{where}: TSCharacters keys are single characters")
        if table.get(key) == value:
            errors.append(f"{where}: OpenCC already maps {key} to {value}; drop the override")
        elif name == "TSCharacters" and key in table:
            errors.append(f"{where}: OpenCC maps {key} to {table[key]}; review before replacing it")
        table[key] = value
        added.append((name, key, value))
    return added


def merge(phrases, characters):
    # Equivalent to OpenCC's short-circuit group of a phrase dictionary followed
    # by a single-character one, since phrase matches are never shorter.
    merged = dict(characters)
    merged.update(phrases)
    return {k: v for k, v in merged.items() if len(k) > 1 or k != v}


def check(tables, overrides, variants, simplify, errors):
    for name, table in tables.items():
        for key, value in table.items():
            if len(key) != len(value):
                errors.append(f"{name}: {key} -> {value} changes the character count")
    st = read_dict("STCharacters")
    st_values = {v for _, _, values in entries(DATA / "STCharacters.txt") for v in values}
    traditional_only = st_values - set(st)
    # A variant the second stage cannot simplify would leak Traditional text
    # (OpenCC 1.3.1 turned 抬 into 擡 this way).
    for key, value in variants.items():
        for source, target in zip(key, value):
            if source != target and target in traditional_only and target not in simplify:
                errors.append(f"variants: {key} -> {value} leaves Traditional {target}")
    for name, key, value in overrides:
        if name == "TSCharacters" and value in traditional_only:
            errors.append(f"overrides: {key} -> {value} is not Simplified")


class Pool:
    def __init__(self):
        self.text = []
        self.size = 0
        self.offsets = {}

    def add(self, string):
        if string not in self.offsets:
            self.offsets[string] = self.size
            self.text.append(string)
            self.size += len(string.encode("utf-8"))
        return self.offsets[string]


def literal(character):
    # Compatibility ideographs look identical to their unified forms and any
    # NFC pass over the source would silently rewrite them.
    if unicodedata.normalize("NFC", character) != character:
        return "\\U%08X" % ord(character)
    return character


def emit_table(name, table):
    pool = Pool()
    rows = []
    for key in sorted(table, key=lambda k: k.encode("utf-8")):
        value = table[key]
        rows.append((pool.add(key), pool.add(value),
                     len(key.encode("utf-8")), len(value.encode("utf-8"))))
    if pool.size > POOL_LIMIT:
        raise SystemExit(f"{name}: pool of {pool.size} bytes exceeds {POOL_LIMIT}")
    max_chars = max(len(k) for k in table)
    lines = [f"static const char k{name}Pool[] ="]
    chunk = ""
    for string in pool.text:
        if len(chunk) + len(string) > 60:
            lines.append(f'    "{chunk}"')
            chunk = ""
        chunk += "".join(literal(c) for c in string)
    lines.append(f'    "{chunk}";')
    lines.append(f"static const TaiwanToSimplifiedEntry k{name}Entries[] = {{")
    for index in range(0, len(rows), 6):
        lines.append("    " + " ".join("{%d, %d, %d, %d}," % row for row in rows[index:index + 6]))
    lines.append("};")
    lines.append(f"static const TaiwanToSimplifiedTable k{name} = {{")
    lines.append(f"    k{name}Pool, k{name}Entries, {len(rows)}, {max_chars}}};")
    return "\n".join(lines)


def generate(overrides_path):
    errors = []
    tables = {
        "TWVariantsRevPhrases": read_dict("TWVariantsRevPhrases"),
        "TSPhrases": read_dict("TSPhrases"),
        "TSCharacters": read_dict("TSCharacters"),
    }
    overrides = apply_overrides(tables, overrides_path, errors)
    normalization = {k: v for k, v in read_dict("CJK_Compatibility_Ideographs").items() if k != v}
    variants = merge(tables["TWVariantsRevPhrases"], reverse_variants("TWVariants"))
    simplify = merge(tables["TSPhrases"], tables["TSCharacters"])
    check(dict(tables, Normalization=normalization), overrides, variants, simplify, errors)
    if errors:
        raise SystemExit("\n".join(errors))
    # Segmentation needs every TSPhrases key, including identity entries.
    segmentation = {k: k for k in tables["TSPhrases"]}
    parts = [
        "// Generated by Scripts/generate-taiwan-to-simplified-table.py; do not edit.",
        "// OpenCC ver.1.4.2 tw2s dictionaries plus TaiwanToSimplifiedOverrides.txt.",
        "",
        emit_table("Normalization", normalization),
        "",
        emit_table("Segmentation", segmentation),
        "",
        emit_table("Variants", variants),
        "",
        emit_table("Simplify", simplify),
        "",
    ]
    return "\n".join(parts)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if the table is stale")
    parser.add_argument("--overrides", type=pathlib.Path, default=OVERRIDES)
    parser.add_argument("--output", type=pathlib.Path, default=OUTPUT)
    args = parser.parse_args()
    text = generate(args.overrides)
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != text:
            print(f"{args.output} is stale; rerun {pathlib.Path(__file__).name}", file=sys.stderr)
            return 1
        return 0
    args.output.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
