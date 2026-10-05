#!/usr/bin/env python3
"""Preflight over the design sheets.

Lists every unfilled cell and every reference between sheets that does not
resolve. Exit code 1 when there are errors; with --release it is also 1 while
anything is still open (unfilled hook cells, unverified rows).

Usage: preflight.py [-v] [--release]
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SHEETS_DIR = ROOT / "sheets"
NAMES = ["entities", "fields", "hooks", "config", "states"]

FIELD_TYPES = {"vec3", "f32", "i32"}
POLICIES = {"lerp", "lerp_angle", "step"}
COMBOS = {
    ("vec3", "lerp"),
    ("vec3", "step"),
    ("f32", "lerp"),
    ("f32", "lerp_angle"),
    ("f32", "step"),
    ("i32", "step"),
}
CONFIG_TYPES = {"i32", "f32", "bool"}
HOOK_STATUS = {"unresolved", "found", "verified"}
HOOK_KINDS = {"native", "memory"}
# Cells a hook row may leave empty while it is still unresolved.
OPEN_OK_HOOK_COLS = {"kind", "target", "game_version"}


def load_sheets():
    sheets = {}
    for name in NAMES:
        path = SHEETS_DIR / f"{name}.json"
        sheets[name] = json.loads(path.read_text(encoding="utf-8"))
    return sheets


def empty(value):
    return value is None or value == ""


def row_id(row):
    return row.get("id", row.get("key", "?"))


def run(sheets):
    """Returns (errors, todo). todo items are (sheet, row, column, reason)."""
    errors = []
    todo = []

    def rows(name):
        return sheets[name]["rows"]

    known = {name: {row_id(r) for r in rows(name)} for name in sheets}

    # 1. Structure, duplicate ids and unfilled cells.
    for name, data in sheets.items():
        cols = data["columns"]
        seen = set()
        for row in data["rows"]:
            who = f"{name}[{row_id(row)}]"
            for extra in sorted(set(row) - set(cols)):
                errors.append(f"{who}: column '{extra}' is not declared")
            if row_id(row) in seen:
                errors.append(f"{who}: duplicate id")
            seen.add(row_id(row))
            for col in cols:
                if col not in row:
                    errors.append(f"{who}.{col}: column missing")
                elif empty(row[col]):
                    if (
                        name == "hooks"
                        and row.get("status") == "unresolved"
                        and col in OPEN_OK_HOOK_COLS
                    ):
                        todo.append((name, row_id(row), col, "unfilled"))
                    else:
                        errors.append(f"{who}.{col}: unfilled")

    # 2. References between sheets.
    def ref(who, col, value, target):
        if empty(value):
            return None
        if value not in known[target]:
            errors.append(f"{who}.{col}: '{value}' does not exist in {target}")
        return value

    used_hooks = set()
    used_config = set()

    for r in rows("entities"):
        who = f"entities[{row_id(r)}]"
        used_hooks.add(ref(who, "handle_hook", r.get("handle_hook"), "hooks"))

    for r in rows("fields"):
        who = f"fields[{row_id(r)}]"
        ref(who, "entity", r.get("entity"), "entities")
        if not str(row_id(r)).startswith(f"{r.get('entity')}."):
            errors.append(f"{who}: id must start with '<entity>.'")
        if r.get("type") not in FIELD_TYPES:
            errors.append(f"{who}.type: '{r.get('type')}' is not one of {sorted(FIELD_TYPES)}")
        if r.get("restore_policy") not in POLICIES:
            errors.append(
                f"{who}.restore_policy: '{r.get('restore_policy')}' is not one of {sorted(POLICIES)}"
            )
        elif (r.get("type"), r.get("restore_policy")) not in COMBOS:
            errors.append(
                f"{who}: policy '{r.get('restore_policy')}' cannot be used with type '{r.get('type')}'"
            )
        for col in ("getter", "setter"):
            used_hooks.add(ref(who, col, r.get(col), "hooks"))
        if r.get("verified") is not True:
            todo.append(("fields", row_id(r), "verified", "not verified"))

    for r in rows("hooks"):
        who = f"hooks[{row_id(r)}]"
        status = r.get("status")
        if status not in HOOK_STATUS:
            errors.append(f"{who}.status: '{status}' is not one of {sorted(HOOK_STATUS)}")
        elif status != "verified":
            todo.append(("hooks", row_id(r), "status", status))
        if not empty(r.get("kind")) and r["kind"] not in HOOK_KINDS:
            errors.append(f"{who}.kind: '{r['kind']}' is not one of {sorted(HOOK_KINDS)}")

    for r in rows("config"):
        who = f"config[{row_id(r)}]"
        if r.get("type") not in CONFIG_TYPES:
            errors.append(f"{who}.type: '{r.get('type')}' is not one of {sorted(CONFIG_TYPES)}")
            continue
        try:
            if not (r["min"] <= r["default"] <= r["max"]):
                errors.append(f"{who}: default is outside min..max")
        except TypeError:
            errors.append(f"{who}: min, max and default must be numbers")

    for r in rows("states"):
        who = f"states[{row_id(r)}]"
        exits = r.get("exits") or []
        if not exits:
            errors.append(f"{who}.exits: needs at least one state")
        for e in exits:
            ref(who, "exits", e, "states")
        for h in r.get("needs_hooks") or []:
            used_hooks.add(ref(who, "needs_hooks", h, "hooks"))
        for k in r.get("uses_config") or []:
            used_config.add(ref(who, "uses_config", k, "config"))

    # 3. Anything defined but never used.
    for h in sorted(known["hooks"]):
        if h not in used_hooks:
            errors.append(f"hooks[{h}]: not used by any entity, field or state")
    for k in sorted(known["config"]):
        if k not in used_config:
            errors.append(f"config[{k}]: not used by any state")

    return errors, todo


def main(argv):
    release = "--release" in argv
    verbose = "-v" in argv or "--verbose" in argv
    sheets = load_sheets()
    errors, todo = run(sheets)

    n_rows = sum(len(d["rows"]) for d in sheets.values())
    n_cells = sum(len(d["rows"]) * len(d["columns"]) for d in sheets.values())
    print(f"Preflight: {len(sheets)} sheets, {n_rows} rows, {n_cells} cells")

    print(f"Errors: {len(errors)}")
    for e in errors:
        print(f"  - {e}")

    print(f"Open (still to fill or verify): {len(todo)}")
    if verbose:
        for sheet, row, col, why in todo:
            print(f"  - {sheet}[{row}].{col}: {why}")
    else:
        groups = {}
        for sheet, _row, col, why in todo:
            groups[(sheet, col, why)] = groups.get((sheet, col, why), 0) + 1
        for (sheet, col, why), n in sorted(groups.items()):
            print(f"  - {sheet}.{col}: {why} x{n}")

    if errors:
        return 1
    if release and todo:
        print("Release build blocked: open items remain.")
        return 1
    print("Structure clean." + (" Nothing open." if not todo else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
