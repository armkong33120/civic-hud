"""Verify actual migration SQL on read-only v1 S22 snapshots, using RAM copies.

Run after :data:compileDebugKotlin so Room's generated v2 schema is available.
This is host SQLite coverage, not an Android Room migration instrumentation test.
"""
import json
from pathlib import Path
import re
import sqlite3

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / "data/src/main/java/com/civichud/maintenance/data"
MIGRATION_SOURCE = (SOURCE / "TrackerMigrations.kt").read_text()
MIGRATION_SQL = [json.loads(line.strip().rstrip(","))
                 for line in MIGRATION_SOURCE.splitlines() if line.strip().startswith('"')]
GENERATED = (ROOT / "data/build/generated/source/kapt/debug/com/civichud/maintenance/data/TrackerDatabase_Impl.java").read_text()
CREATE_SQL = [json.loads('"' + raw + '"') for raw in re.findall(
    r'SQLite\.execSQL\(connection, "((?:[^"\\]|\\.)*)"\)', GENERATED)]
TABLES = ("vehicles", "maintenance_rules", "service_logs", "notification_reservations")


def rows(db, table):
    return [dict(row) for row in db.execute("SELECT * FROM " + table + " ORDER BY id")]


def schema(db):
    result = {}
    for table in (*TABLES, "feedback_outbox"):
        result[table] = {
            "columns": sorted(tuple(r)[1:] for r in db.execute("PRAGMA table_info(" + table + ")")),
            "indexes": sorted((r[1], r[2], tuple(x[2] for x in db.execute("PRAGMA index_info(" + r[1] + ")")))
                              for r in db.execute("PRAGMA index_list(" + table + ")")),
        }
    return result


fresh = sqlite3.connect(":memory:")
for statement in CREATE_SQL:
    if statement.startswith(("CREATE TABLE", "CREATE INDEX", "CREATE UNIQUE INDEX")):
        fresh.execute(statement)
expected_schema = schema(fresh)
assert "manufacturer" in {row[1] for row in fresh.execute("PRAGMA table_info(vehicles)")}, "Compile v2 first"

snapshots = (ROOT / "verification/vehicle_tracker.db",
             ROOT / "verification/vehicle_tracker_simulation.db",
             ROOT / "verification/final-state/vehicle_tracker_simulation.db")
results = []
for path in snapshots:
    original = sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True)
    assert original.execute("PRAGMA user_version").fetchone()[0] == 1
    migrated = sqlite3.connect(":memory:")
    original.backup(migrated)
    original.close()
    migrated.row_factory = sqlite3.Row
    before = {table: rows(migrated, table) for table in TABLES}
    with migrated:
        for statement in MIGRATION_SQL:
            migrated.execute(statement)
    for table in TABLES:
        after = rows(migrated, table)
        for old, new in zip(before[table], after):
            assert all(new[key] == value for key, value in old.items()), (path, table)
        assert len(before[table]) == len(after)
    vehicle = rows(migrated, "vehicles")[0]
    assert all(vehicle[field] == "" for field in ("manufacturer", "model", "registration", "fuelType"))
    assert vehicle["modelYear"] is None
    assert rows(migrated, "feedback_outbox") == []
    assert schema(migrated) == expected_schema, "Migrated schema must match Room's fresh v2 schema"
    assert migrated.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
    results.append({"snapshot": str(path.relative_to(ROOT)), "preserved_row_counts":
                    {table: len(values) for table, values in before.items()},
                    "all_existing_values_preserved": True, "room_v2_schema_matches": True,
                    "profile_defaults_empty": True, "feedback_outbox_empty": True,
                    "integrity_check": "ok"})
    migrated.close()
fresh.close()
print(json.dumps({"evidence": "Host SQLite RAM copies; actual v1 S22 inputs; not Android instrumentation",
                  "snapshots": results}, ensure_ascii=False, indent=2))
