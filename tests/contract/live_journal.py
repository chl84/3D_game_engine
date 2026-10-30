#!/usr/bin/env python3
"""Durable live CommandBus records survive termination and failed appends fail closed."""
from __future__ import annotations

import json
import os
import socket
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = json.loads((ROOT / "schemas/live-command-record@1.schema.json").read_text())


def main():
    engine = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="engine-live-journal-") as temporary:
        project = Path(temporary) / "project"

        def cli(*args):
            return subprocess.run([engine, *map(str, args), "--json"], check=True,
                                  capture_output=True, text=True, timeout=30)

        cli("project", "create", project)
        session = project / ".engine/live.json"

        def start():
            process = subprocess.Popen([engine, "live", "serve", str(project), "--json"],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline and not session.exists() and process.poll() is None:
                    time.sleep(0.02)
                assert process.poll() is None and session.exists(), "live startup failed"
                startup = json.loads(process.stdout.readline())
                discovery = json.loads(session.read_text())
                journal = project / startup["data"]["commandLogFile"]
                assert journal.parent == project / ".engine" and journal.suffix == ".jsonl"
                assert journal.exists()
                if os.name != "nt":
                    assert stat.S_IMODE(journal.stat().st_mode) == 0o600

                def rpc(method, params=None):
                    payload = {"jsonrpc": "2.0", "id": 1, "protocolVersion": 1,
                               "method": method, "params": params or {}, "token": discovery["token"]}
                    with socket.create_connection(("127.0.0.1", discovery["port"]), timeout=5) as connection:
                        connection.sendall(json.dumps(payload).encode() + b"\n")
                        raw = b""
                        while chunk := connection.recv(65536):
                            raw += chunk
                    return json.loads(raw)["result"]

                return process, journal, discovery["token"], rpc
            except BaseException:
                process.kill()
                process.wait(timeout=10)
                raise

        def records(path):
            raw = path.read_bytes()
            assert raw.endswith(b"\n")
            rows = [json.loads(line) for line in raw.splitlines()]
            for row in rows:
                jsonschema.Draft7Validator(SCHEMA).validate(row)
            return raw, rows

        process, journal, credential, rpc = start()
        try:
            result = rpc("command", {"name": "scene.entity.create@1", "arguments": {"name": "Crash survivor"}})
            assert result["status"] == "ok"
            # The file is complete before acknowledgement, while the server is alive.
            before, rows = records(journal)
            assert len(rows) == 2 and [r["event"] for r in rows] == ["started", "completed"]
            assert rows[0]["dispatch"] == rows[1]["dispatch"] == 1
            assert rows[0]["session"] == rows[1]["session"]
            assert rows[1]["sequence"] == 1 and rows[1]["result"] == result
            assert rows[1]["arguments"]["name"] == "Crash survivor"
            rejected = rpc("command", {"name": "scene.entity.create@1", "arguments": {"extra": True}})
            assert rejected["status"] == "error"
            surviving, rows = records(journal)
            assert len(rows) == 4 and rows[-1]["result"] == rejected
            assert credential.encode() not in surviving
            process.kill()  # SIGKILL / TerminateProcess, no destructor or shutdown flush.
            process.wait(timeout=10)
            assert journal.read_bytes() == surviving
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=10)
        cli("project", "unlock", project, "--force")
        session.unlink()  # Old discovery is intentionally stale after hard termination.
        second, new_journal, new_credential, rpc = start()
        try:
            assert new_journal != journal and new_credential != credential
            assert journal.read_bytes() == surviving, "restart overwrote the crashed session"
            assert rpc("command", {"name": "scene.validate@1"})["status"] == "ok"
            records(new_journal)
            # Make append fail before the next command: no mutation may execute.
            backup = new_journal.with_suffix(".backup")
            new_journal.rename(backup)
            new_journal.mkdir()
            failed = rpc("command", {"name": "scene.save@1", "arguments": {"path": "scenes/never.scene.json"}})
            assert failed["status"] == "error" and failed["error"]["code"] == "E_IO"
            assert not (project / "scenes/never.scene.json").exists()
            assert second.wait(timeout=10) == 0
            assert not session.exists() and not (project / ".engine/lock").exists()
            assert journal.read_bytes() == surviving
            assert len(records(backup)[1]) == 2
        finally:
            if second.poll() is None:
                second.kill()
            second.wait(timeout=10)
    print("Live JSONL intent/result sync, hard-crash survival, restart retention and append-failure exclusion passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
