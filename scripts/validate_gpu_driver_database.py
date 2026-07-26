#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATABASE = ROOT / "docs" / "gpu-driver-database.json"
HEADER = (
    ROOT
    / "app"
    / "src"
    / "main"
    / "cpp"
    / "pcsx2"
    / "pcsx2"
    / "GS"
    / "Renderers"
    / "Common"
    / "GSGPUProfile.h"
)
RULES_CPP = HEADER.with_name("GSGPUDriverProfile.cpp")


def enum_members(source: str, enum_name: str) -> set[str]:
    match = re.search(
        rf"enum\s+class\s+{re.escape(enum_name)}\s*:\s*u8\s*\{{(?P<body>.*?)\}};",
        source,
        flags=re.DOTALL,
    )
    if not match:
        raise ValueError(f"Cannot find enum {enum_name}")

    members = set()
    for raw_line in match.group("body").splitlines():
        name = raw_line.split("//", 1)[0].strip().rstrip(",")
        if name and name != "Count":
            members.add(name)
    return members


def fail(message: str, errors: list[str]) -> None:
    errors.append(message)


def main() -> int:
    errors: list[str] = []
    if not DATABASE.exists():
        print(
            "Private GPU driver database is not present; "
            "nothing to validate in this checkout."
        )
        return 0

    database = json.loads(DATABASE.read_text(encoding="utf-8"))
    header = HEADER.read_text(encoding="utf-8")
    rules_cpp = RULES_CPP.read_text(encoding="utf-8")

    if database.get("schemaVersion") != 1:
        fail("schemaVersion must be 1", errors)
    if database.get("databaseVersion") != 1:
        fail("databaseVersion must match MobileDriverProfile::DATABASE_VERSION (1)", errors)

    rules = database.get("rules", [])
    json_ids = [rule.get("id") for rule in rules]
    if len(json_ids) != len(set(json_ids)):
        fail("Rule IDs in JSON are not unique", errors)

    cpp_ids = re.findall(r'^\s*\{"([a-z0-9-]+)"', rules_cpp, flags=re.MULTILINE)
    if set(json_ids) != set(cpp_ids):
        missing_json = sorted(set(cpp_ids) - set(json_ids))
        missing_cpp = sorted(set(json_ids) - set(cpp_ids))
        fail(f"Rule ID mismatch; missing in JSON={missing_json}, missing in C++={missing_cpp}", errors)

    valid_bugs = enum_members(header, "DriverBug")
    valid_workarounds = enum_members(header, "DriverWorkaround")
    valid_statuses = {"active", "partial", "catalogued"}
    sources = database.get("sources", {})
    used_bugs: set[str] = set()
    used_workarounds: set[str] = set()

    for rule in rules:
        rule_id = rule.get("id", "<missing>")
        if rule.get("integrationStatus") not in valid_statuses:
            fail(f"{rule_id}: invalid integrationStatus", errors)
        workarounds = rule.get("workarounds", [])
        integrations = rule.get("activeIntegrations", [])
        if len(workarounds) != len(set(workarounds)):
            fail(f"{rule_id}: workaround names must be unique", errors)
        if len(integrations) != len(set(integrations)):
            fail(f"{rule_id}: active integration names must be unique", errors)
        if rule.get("integrationStatus") == "active" and set(integrations) != set(workarounds):
            missing = sorted(set(workarounds) - set(integrations))
            extra = sorted(set(integrations) - set(workarounds))
            fail(
                f"{rule_id}: active rule must integrate every workaround; "
                f"missing={missing}, extra={extra}",
                errors,
            )
        if rule.get("integrationStatus") == "partial" and not integrations:
            fail(f"{rule_id}: partial rule requires activeIntegrations", errors)
        if rule.get("integrationStatus") == "catalogued" and integrations:
            fail(f"{rule_id}: catalogued rule cannot have activeIntegrations", errors)
        for bug in rule.get("bugs", []):
            used_bugs.add(bug)
            if bug not in valid_bugs:
                fail(f"{rule_id}: unknown DriverBug {bug}", errors)
        for workaround in workarounds:
            used_workarounds.add(workaround)
            if workaround not in valid_workarounds:
                fail(f"{rule_id}: unknown DriverWorkaround {workaround}", errors)
        for integration in integrations:
            if integration not in workarounds:
                fail(f"{rule_id}: active integration {integration} is not a rule workaround", errors)
        if not rule.get("sources"):
            fail(f"{rule_id}: at least one source is required", errors)
        for source_id in rule.get("sources", []):
            if source_id not in sources:
                fail(f"{rule_id}: unknown source {source_id}", errors)

    if used_bugs != valid_bugs:
        fail(
            "DriverBug coverage mismatch; "
            f"unused={sorted(valid_bugs - used_bugs)}, unknown={sorted(used_bugs - valid_bugs)}",
            errors,
        )
    if used_workarounds != valid_workarounds:
        fail(
            "DriverWorkaround coverage mismatch; "
            f"unused={sorted(valid_workarounds - used_workarounds)}, "
            f"unknown={sorted(used_workarounds - valid_workarounds)}",
            errors,
        )

    for source_id, source in sources.items():
        revision = source.get("revision", "")
        if not revision:
            fail(f"{source_id}: revision is required", errors)
        if not source.get("url", "").startswith("https://"):
            fail(f"{source_id}: an HTTPS source URL is required", errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print(
        f"GPU driver database OK: {len(rules)} rules, "
        f"{len(valid_bugs)} bug flags, {len(valid_workarounds)} workaround flags, "
        f"{len(sources)} pinned sources."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
