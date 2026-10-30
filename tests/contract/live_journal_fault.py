#!/usr/bin/env python3
"""Linux non-sanitized integration: actual fsync failures must never acknowledge success."""
from __future__ import annotations

import json
import os
import socket
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    engine, fault_library = map(lambda value: str(Path(value).resolve()), sys.argv[1:3])
    for mode, notification in (("started", False), ("completed", False),
                               ("completed", True), ("none", False)):
        with tempfile.TemporaryDirectory(prefix="engine-journal-fault-") as temporary:
            project = Path(temporary) / "project"
            subprocess.run([engine, "project", "create", str(project), "--json"],
                           capture_output=True, check=True, timeout=20)
            process = subprocess.Popen([engine, "live", "serve", str(project), "--json"],
                                       env=dict(os.environ, LD_PRELOAD=fault_library,
                                                ENGINE_JOURNAL_FAULT=mode),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                ready = json.loads(process.stdout.readline())
                discovery = json.loads((project / ".engine/live.json").read_text())

                def rpc(method, params, notify=False):
                    request = {"jsonrpc": "2.0", "protocolVersion": 1, "method": method,
                               "params": params, "token": discovery["token"]}
                    if not notify:
                        request["id"] = 1
                    with socket.create_connection(("127.0.0.1", discovery["port"]), timeout=5) as connection:
                        connection.sendall(json.dumps(request).encode() + b"\n")
                        response = b""
                        while chunk := connection.recv(65536):
                            response += chunk
                    return response

                output = project / "scenes/fault.scene.json"
                raw = rpc("command", {"name": "scene.save@1",
                                      "arguments": {"path": "scenes/fault.scene.json"}}, notification)
                if mode == "none":
                    assert json.loads(raw)["result"]["status"] == "ok"
                    assert process.poll() is None
                    rpc("shutdown", {})
                    assert process.wait(timeout=10) == 0
                    assert process.stderr.read() == ""
                else:
                    if notification:
                        assert raw == b""
                    else:
                        result = json.loads(raw)["result"]
                        assert result["status"] == "error" and result["error"]["code"] == "E_IO"
                    assert process.wait(timeout=10) == 6
                    assert process.stderr.read() == "error: live command journal failed; session stopped\n"
                assert output.exists() == (mode != "started")
                journal = project / ready["data"]["commandLogFile"]
                wire = journal.read_bytes()
                records = [json.loads(line) for line in wire.splitlines()]
                expected = ["started"] if mode == "started" else ["started", "completed"]
                assert [record["event"] for record in records] == expected
                assert discovery["token"].encode() not in wire
                assert not (project / ".engine/live.json").exists()
                assert not (project / ".engine/lock").exists()
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=10)
    print("Started/completed fsync faults, uncertain outcome, notification diagnostics and no-fault control passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
