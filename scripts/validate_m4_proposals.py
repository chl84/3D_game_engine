#!/usr/bin/env python3
"""Validate unadopted M4 proposal envelopes and deliberate invalid controls."""
from __future__ import annotations

import copy
import json
from pathlib import Path

import jsonschema
from referencing import Registry, Resource


def main() -> None:
    root = Path(__file__).resolve().parents[1] / "docs/proposals/m4"
    schemas = [json.loads(path.read_text()) for path in root.glob("*.schema.json")]
    registry = Registry().with_resources(
        (schema["$id"], Resource.from_contents(schema)) for schema in schemas
    )
    for schema in schemas:
        jsonschema.Draft7Validator.check_schema(schema)
    validators = {
        schema["$id"].rsplit("/", 1)[-1]: jsonschema.Draft7Validator(schema, registry=registry)
        for schema in schemas
    }

    def load(name: str):
        return json.loads((root / "examples" / name).read_text())

    project, scene = load("project.json"), load("scene.json")
    records = [json.loads(line) for line in (root / "examples/replay.jsonl").read_text().splitlines()]
    examples = [("project@2", project), ("scene@4", scene)]
    examples.extend(("replay@1", record) for record in records)
    for name in ("game-profile-instance.json", "game-profile-session.json"):
        examples.append(("game-profile@1", load(name)))
    for name, example in examples:
        validators[name + ".schema.json"].validate(example)

    controls = []

    def reject(name: str, source, mutate) -> None:
        candidate = copy.deepcopy(source)
        mutate(candidate)
        assert not validators[name + ".schema.json"].is_valid(candidate), name
        controls.append(name)

    reject("project@2", project, lambda v: v.update(formatVersion=1))
    reject("project@2", project, lambda v: v.update(unknown=True))
    reject("project@2", project, lambda v: v["settings"]["window"].update(mode="invalid"))
    profile = project["gameProfile"]
    reject("game-profile@1", profile, lambda v: v.update(tickRate=30))
    reject("game-profile@1", profile, lambda v: v["lua"].update(vmTopology="unspecified"))
    reject("game-profile@1", profile, lambda v: v["lua"]["limits"].update(luaInstanceBytes=0))
    reject("scene@4", scene, lambda v: v["entities"][0]["components"].pop("engine.scene.Transform"))
    reject("scene@4", scene, lambda v: v["entities"][0].update(id="bad-uuid"))
    reject("scene@4", scene, lambda v: v["entities"][0]["components"][
        "engine.physics.CharacterController"].update(radius=-1))
    reject("scene@4", scene, lambda v: v["entities"][0]["components"][
        "engine.script.Script"].update(extra=True))
    reject("replay@1", records[0], lambda v: v.update(tickRate=30))
    reject("replay@1", records[0], lambda v: v.update(profileHash="bad-hash"))
    reject("replay@1", records[1], lambda v: v["input"]["axes"].update(moveX=2))
    reject("replay@1", records[1], lambda v: v.update(tick=-1))
    reject("replay@1", records[-1], lambda v: v.update(complete=False))
    print(f"PROPOSALS ONLY: {len(schemas)} valid schemas, {len(examples)} examples, "
          f"{len(controls)} rejected controls; runtime/semantic validation not implemented")


if __name__ == "__main__":
    main()
