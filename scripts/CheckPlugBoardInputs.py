"""Verify packaged tutorial artwork and verbatim owner-authored response imports."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
for personality, source in (("default", "Norm"), ("uwu", "Horu")):
    text = (root / f"docs/plug-board/PlugBoard_{source}_Responses.txt").read_text(encoding="utf-8-sig")
    expected = {}
    for line in text.splitlines():
        row = re.fullmatch(r"([A-Z_]+)\s*\|\s*([a-z_]+)\s*\|\s*(.+)", line)
        if row:
            expected.setdefault("plug_board_" + row[1].lower(), []).append((row[2], row[3].strip()))
    actual = json.loads((root / f"personalities/{personality}/dialogue/responses.json").read_text(encoding="utf-8"))["events"]
    assert len(expected) == 10 and all(len(rows) == 4 for rows in expected.values()), source
    for event, rows in expected.items():
        assert [(item["expression"], item["text"]) for item in actual[event]] == rows, (source, event)
        assert all(item["fallback_expression"] == "idle" for item in actual[event]), (source, event)

artwork = root / "assets/tutorial/apps"
manifest = json.loads((artwork / "provenance.json").read_text(encoding="utf-8"))
assert len(manifest["files"]) == 6
for item in manifest["files"]:
    assert hashlib.sha256((artwork / item["name"]).read_bytes()).hexdigest() == item["packaged_sha256"], item["name"]
print("PASS: 80 verbatim Plug Board responses and six packaged tutorial icons")
