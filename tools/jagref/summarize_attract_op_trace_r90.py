#!/usr/bin/env python3
"""Read-only summary of the JagMachine post-field MJP/OP timing witness.

A phase/ACK is not a published frame. Display and staged-list data are
reported independently; no claims about real Jaguar cycle timing.
"""
import argparse
import csv
from pathlib import Path


def main(path: Path) -> None:
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit("empty trace")
    expected = ("field", "stage_7fb4", "image_7fa4", "pending_data_4fc40",
                "staged_data_50c40", "frame_hash_fnv64")
    for field in expected:
        if field not in rows[0]:
            raise SystemExit(f"missing required column: {field}")
    previous = None
    mismatches = 0
    staging_changes = 0
    frame_changes = 0
    stage_changes = 0
    first_progress = None
    first_staged = None
    first_changed_frame = None
    print("Field / GPU stage / published GPU buffer / staged OP buffer / field changed")
    for row in rows:
        field = int(row["field"])
        stage = int(row["stage_7fb4"])
        published = row["image_7fa4"]
        staged = row["staged_data_50c40"]
        pending = row["pending_data_4fc40"]
        if staged != pending:
            mismatches += 1
        if previous:
            is_stage = row["stage_7fb4"] != previous["stage_7fb4"]
            is_staging = staged != previous["staged_data_50c40"]
            is_frame = row["frame_hash_fnv64"] != previous["frame_hash_fnv64"]
            stage_changes += is_stage
            staging_changes += is_staging
            frame_changes += is_frame
            if is_stage and stage == 1 and first_progress is None:
                first_progress = field
            if first_progress is not None and is_staging and first_staged is None:
                first_staged = field
            if first_progress is not None and is_frame and first_changed_frame is None:
                first_changed_frame = field
            if is_stage or is_staging or is_frame:
                print(f"{field:5} / {stage:2} / ${published} / ${staged} / {'YES' if is_frame else 'no'}")
        previous = row
    print(f"SUMMARY: fields={len(rows)}, stage_changes={stage_changes}, "
          f"staged_pointer_changes={staging_changes}, "
          f"field_hash_changes={frame_changes}, pending_vs_staged_mismatches={mismatches}")
    print(f"FIRST STEP: GPU stage-1 field={first_progress}, staged pointer first changes "
          f"on field={first_staged}, full-field hash first changes on field={first_changed_frame}")
    print("LIMIT: JagMachine has no shared-bus contention model. These are post-field "
          "observations, not actual ROM hardware scanline timings.")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("trace", type=Path)
    main(p.parse_args().trace)
