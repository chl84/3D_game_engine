#!/usr/bin/env python3
"""Live-only authoring plus transport/auth/sandbox/locking regression tests."""
from __future__ import annotations
import base64
import json
import os
import shutil
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path
import jsonschema

ROOT = Path(__file__).resolve().parents[2]
ENVELOPE = json.loads((ROOT / "schemas/command-result@1.schema.json").read_text())
RESPONSE = json.loads((ROOT / "schemas/jsonrpc-response@1.schema.json").read_text())


def alter_glb(path):
    raw = path.read_bytes()
    size = struct.unpack_from("<I", raw, 12)[0]
    document = json.loads(raw[20:20 + size])
    document["nodes"][0]["translation"] = [0, 0.5, 0]
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    payload = struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + raw[20 + size:]
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, len(payload) + 12) + payload)


def main():
    engine = str(Path(sys.argv[1]).resolve())

    def cli(*args, code=0):
        result = subprocess.run([engine, *map(str, args), "--json"], capture_output=True, text=True, timeout=45)
        assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
        document = json.loads(result.stdout)
        jsonschema.Draft7Validator(ENVELOPE).validate(document)
        return document

    with tempfile.TemporaryDirectory(prefix="engine-live-") as temporary:
        tmp = Path(temporary)
        project = tmp / "project"
        cli("project", "create", project)
        shutil.copyfile(ROOT / "tests/assets/m3/blender_cube.glb", project / "model.glb")
        outside = tmp / "outside.glb"
        shutil.copyfile(project / "model.glb", outside)
        external = cli("asset", "import", project, outside)["data"]
        alter_glb(outside)
        session = project / ".engine/live.json"
        stderr = (tmp / "server-stderr.txt").open("w+")
        process = subprocess.Popen([engine, "live", "serve", str(project), "--io-timeout-ms", "400", "--json"],
                                   stdout=subprocess.PIPE, stderr=stderr, text=True)
        discovery = None
        completed = False
        known_credentials = []
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and not session.exists() and process.poll() is None:
                time.sleep(0.02)
            if process.poll() is not None:
                raise AssertionError("server startup failed: " + process.stdout.read())
            assert session.exists(), "server startup timed out"
            discovery = json.loads(session.read_text())
            credential = discovery["token"]
            known_credentials.append(credential)
            port = discovery["port"]
            assert discovery["protocolVersion"] == 1 and len(credential) == 64
            startup = json.loads(process.stdout.readline())
            assert credential not in json.dumps(startup)
            if os.name != "nt":
                assert stat.S_IMODE(session.stat().st_mode) == 0o600
                assert session.stat().st_uid == os.getuid()
            else:
                script = '''$a=Get-Acl -LiteralPath $env:ENGINE_TEST_DISCOVERY
if(-not $a.AreAccessRulesProtected){exit 1}
$u=[System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
if($a.GetOwner([System.Security.Principal.SecurityIdentifier]).Value -ne $u){exit 3}
$r=@($a.GetAccessRules($true,$true,[System.Security.Principal.SecurityIdentifier]))
if($r.Count -ne 1 -or $r[0].IdentityReference.Value -ne $u -or $r[0].IsInherited -or $r[0].AccessControlType -ne 'Allow'){exit 2}'''
                subprocess.run(["powershell", "-NoProfile", "-Command", script],
                               env=dict(os.environ, ENGINE_TEST_DISCOVERY=str(session)), check=True, timeout=15)

            def connect():
                return socket.create_connection(("127.0.0.1", port), timeout=5)

            def exchange(payload):
                with connect() as connection:
                    try:
                        connection.sendall(payload)
                        parts = []
                        while True:
                            chunk = connection.recv(65536)
                            if not chunk:
                                break
                            parts.append(chunk)
                        return b"".join(parts)
                    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
                        return b""

            sequence = 0

            def request(method, params=None, credential_value=None):
                nonlocal sequence
                sequence += 1
                document = {"jsonrpc": "2.0", "id": sequence, "method": method,
                            "params": params or {}, "protocolVersion": 1,
                            "token": credential if credential_value is None else credential_value}
                raw = exchange(json.dumps(document).encode() + b"\n")
                assert raw, (method, "no response")
                response = json.loads(raw)
                jsonschema.Draft7Validator(RESPONSE).validate(response)
                if "result" in response:
                    assert response["id"] == sequence
                    jsonschema.Draft7Validator(ENVELOPE).validate(response["result"])
                return response

            def rpc(method, params=None):
                result = request(method, params)
                assert "result" in result and result["result"]["status"] == "ok", result
                return result["result"]["data"]

            def command(name, arguments=None):
                return rpc("command", {"name": name, "arguments": arguments or {}})

            assert rpc("ping")["alive"] is True
            assert cli("live", "call", project, "--method", "ping")["data"]["alive"] is True
            if os.name == "nt":
                assert cli("live", "call", str(project).swapcase(), "--method", "ping")["data"]["alive"] is True
            # Binding to INADDR_ANY would incorrectly accept this alternate destination.
            try:
                with socket.create_connection(("127.0.0.2", port), timeout=0.2):
                    raise AssertionError("listener was not restricted to127.0.0.1")
            except OSError:
                pass
            denied = {"jsonrpc": "2.0", "id": None, "error": {"code": -32001, "message": "Unauthorized"}}
            assert request("ping", credential_value="wrong") == denied
            wrong_same_length = ("0" if credential[0] != "0" else "1") + credential[1:]
            assert request("ping", credential_value=wrong_same_length) == denied
            assert request("unknown-secret-method", {"echo": credential}, credential_value="wrong") == denied
            missing = {"jsonrpc": "2.0", "id": 1, "method": "ping", "protocolVersion": 1}
            assert json.loads(exchange(json.dumps(missing).encode() + b"\n")) == denied
            assert json.loads(exchange(b'{"token":"a","token":"b"}\n'))["error"]["code"] == -32700
            assert json.loads(exchange(b'{broken\n'))["error"]["code"] == -32700
            assert json.loads(exchange(b"[" * 64 + b"]" * 64 + b"\n"))["error"]["code"] == -32700
            assert exchange(b"x" * 65537 + b"\n") == b""
            assert request("ping", {"extra": True})["error"]["code"] == -32602
            notification = {"jsonrpc": "2.0", "method": "ping", "protocolVersion": 1, "token": credential}
            assert exchange(json.dumps(notification).encode() + b"\n") == b""
            before = rpc("inspect", {"target": "log"})["total"]
            with connect() as connection:
                partial = {"jsonrpc": "2.0", "id": 1, "method": "command", "protocolVersion": 1,
                           "token": credential, "params": {"name": "scene.entity.create@1", "arguments": {"name": "Never"}}}
                connection.sendall(json.dumps(partial).encode())  # No frame delimiter: never dispatch.
            with connect():
                pass
            assert rpc("ping")["alive"]
            assert rpc("inspect", {"target": "log"})["total"] == before
            # A complete request followed by RST exercises response-write cleanup.
            with connect() as aborted:
                aborted.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                   struct.pack("hh" if os.name == "nt" else "ii", 1, 0))
                aborted.sendall(json.dumps(dict(notification, id=77)).encode() + b"\n")
            assert rpc("ping")["alive"]
            # Idle clients do not block another client, and their absolute deadline expires.
            with connect() as idle:
                idle.sendall(b"{")
                assert rpc("ping")["alive"]
                assert idle.recv(1) == b""
            # Incoming fragments must not extend the absolute receive deadline.
            with connect() as trickle:
                for _ in range(6):
                    try:
                        trickle.sendall(b" ")
                    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
                        break
                    time.sleep(0.1)
                trickle.settimeout(0.15)
                try:
                    assert trickle.recv(1) == b""
                except (ConnectionResetError, ConnectionAbortedError):
                    pass
            idle_clients = [connect() for _ in range(8)]
            try:
                assert exchange(json.dumps(dict(notification, id=99)).encode() + b"\n") == b""
                time.sleep(0.5)
                for idle in idle_clients:
                    try:
                        assert idle.recv(1) == b""
                    except ConnectionResetError:
                        pass
            finally:
                for idle in idle_clients:
                    idle.close()
            assert rpc("ping")["alive"]
            # Lifetime project lock excludes every offline writer, including screenshot/force-unlock.
            for args in (("entity", "create", project, "--name", "Blocked"),
                         ("asset", "import", project, project / "model.glb"),
                         ("screenshot", project, "--out", "blocked.png", "--width", 64, "--height", 64),
                         ("project", "unlock", project, "--force"),
                         ("live", "serve", project)):
                result = cli(*args, code=5)
                assert result["error"]["code"] == "E_CONFLICT"
            assert json.loads(session.read_text())["token"] == credential
            # Shared schema surface, then exclusively live authoring.
            schemas = rpc("inspect", {"target": "schema"})
            assert schemas == cli("schema")["data"]
            camera = command("scene.entity.create@1", {"name": "Camera"})["id"]
            command("scene.component.add@1", {"id": camera, "component": "engine.scene.Camera"})
            command("scene.component.set@1", {"id": camera, "component": "engine.scene.Transform", "value": {"position": [0, 0, 7]}})
            imported = command("asset.import@1", {"source": "model.glb"})
            asset_id = imported["assetId"]
            assert len(command("asset.list@1")["assets"]) == 2
            assert command("asset.inspect@1", {"assetId": asset_id})["cache"]["state"] == "ready"
            assert command("asset.validate@1", {"assetId": asset_id})["valid"]
            fallback = command("asset.reimport@1", {"assetId": external["assetId"]})
            assert fallback["sourceHash"] == external["sourceHash"], "live reimport read an external original"
            for x in (-1.5, 1.5):
                command("asset.place@1", {"assetId": asset_id, "position": [x, 0, 0]})
            scene = rpc("inspect")
            assert len(scene["entities"]) == 3
            screenshot = rpc("screenshot", {"out": "screenshots/live.png", "width": 128, "height": 96})
            png = base64.b64decode(screenshot["pngBase64"], validate=True)
            assert png[:8] == b"\x89PNG\r\n\x1a\n" and png == (project / "screenshots/live.png").read_bytes()
            # The shared delete/undo path must restore authored light components too.
            sun = command("scene.entity.create@1", {"name": "Sun"})["id"]
            command("scene.component.add@1", {"id": sun, "component": "engine.scene.DirectionalLight"})
            command("scene.component.set@1", {"id": sun, "component": "engine.scene.DirectionalLight", "value": {"direction": [0, -1, -1]}})
            lit = rpc("screenshot", {"width": 128, "height": 96})["pngBase64"]
            assert base64.b64decode(lit) != png
            command("scene.entity.delete@1", {"id": sun})
            command("history.undo@1")
            assert rpc("screenshot", {"width": 128, "height": 96})["pngBase64"] == lit
            command("scene.entity.delete@1", {"id": sun})
            point = command("scene.entity.create@1", {"name": "Reserved point"})["id"]
            command("scene.component.add@1", {"id": point, "component": "engine.scene.PointLight"})
            command("scene.component.set@1", {"id": point, "component": "engine.scene.PointLight", "value": {"intensity": 3}})
            command("scene.entity.delete@1", {"id": point})
            command("history.undo@1")
            restored_point = next(e for e in rpc("inspect")["entities"] if e["id"] == point)
            assert restored_point["components"]["engine.scene.PointLight"]["intensity"] == 3
            command("scene.entity.delete@1", {"id": point})
            command("scene.save@1", {"path": "scenes/main.scene.json"})
            saved = (project / "scenes/main.scene.json").read_bytes()
            extra = command("scene.entity.create@1", {"name": "Unsaved"})
            command("history.undo@1")
            command("history.redo@1")
            assert len(rpc("inspect")["entities"]) == 4
            command("scene.load@1", {"path": "scenes/main.scene.json"})
            assert len(rpc("inspect")["entities"]) == 3
            assert (project / "scenes/main.scene.json").read_bytes() == saved
            no_history = request("command", {"name": "history.undo@1"})["result"]
            assert no_history["status"] == "error"
            assert extra["id"] not in json.dumps(rpc("inspect"))
            alter_glb(project / "model.glb")
            rebuilt = command("asset.reimport@1", {"assetId": asset_id})
            assert rebuilt["assetId"] == asset_id and rebuilt["cacheKey"] != imported["cacheKey"]
            assert base64.b64decode(rpc("screenshot", {"width": 128, "height": 96})["pngBase64"]) != png
            assert (project / "scenes/main.scene.json").read_bytes() == saved
            # Every client-controlled filesystem path is sandboxed; protected outputs cannot clobber discovery.
            for name, arguments in (
                ("asset.import@1", {"source": "../outside.glb"}),
                ("asset.import@1", {"source": str(outside)}),
                ("asset.import@1", {"source": "model.glb\0ignored"}),
                ("scene.save@1", {"path": "../escape.scene.json"}),
                ("scene.load@1", {"path": str(tmp / "escape.scene.json")}),
            ):
                result = request("command", {"name": name, "arguments": arguments})["result"]
                assert result["status"] == "error" and result["error"]["code"] == "E_VALIDATION", result
            for out in ("../escape.png", str(tmp / "escape.png")):
                assert request("screenshot", {"out": out})["result"]["error"]["code"] == "E_VALIDATION"
            assert request("screenshot", {"out": ".engine/live.json"})["error"]["code"] == -32602
            for dimensions in ({"width": 513}, {"height": 513}, {"frames": 17}, {"frames": 0}, {"width": 1.5}):
                assert request("screenshot", dimensions)["error"]["code"] == -32602
            assert not (tmp / "escape.png").exists() and not (tmp / "escape.scene.json").exists()
            # Symlinks cannot turn a project-relative request into external I/O.
            try:
                (project / "linked.glb").symlink_to(outside)
                external_dir = tmp / "external-output"
                external_dir.mkdir()
                (project / "linked-output").symlink_to(external_dir, target_is_directory=True)
                (project / "alias.scene.json").symlink_to(session)
            except OSError:
                print("Directory/file symlink fixture unavailable on this platform")
            else:
                rejected = request("command", {"name": "asset.import@1", "arguments": {"source": "linked.glb"}})["result"]
                assert rejected["error"]["code"] == "E_VALIDATION"
                assert request("screenshot", {"out": "linked-output/escape.png"})["result"]["error"]["code"] == "E_VALIDATION"
                assert not (external_dir / "escape.png").exists()
                rejected = request("command", {"name": "scene.save@1", "arguments": {"path": "alias.scene.json"}})["result"]
                assert rejected["error"]["code"] == "E_VALIDATION"
                assert json.loads(session.read_text())["token"] == credential
            entries = rpc("inspect", {"target": "log", "limit": 64})["entries"]
            assert any(e["name"] == "asset.place@1" and e["arguments"]["assetId"] == asset_id for e in entries)
            assert any(e["name"] == "scene.component.set@1" for e in entries)
            assert credential not in json.dumps(entries), "authentication credential entered command log"
            # Fill the serialized log budget without exhausting the entry budget.
            total_before_budget = rpc("inspect", {"target": "log"})["total"]
            accepted = 0
            for _ in range(400):
                response = request("command", {"name": "scene.entity.create@1", "arguments": {"name": "B" * 62000}})
                if "error" in response:
                    assert response["error"]["code"] == -32002
                    break
                assert response["result"]["status"] == "ok"
                accepted += 1
            else:
                raise AssertionError("serialized command-log budget was not enforced")
            budget_log = rpc("inspect", {"target": "log", "offset": total_before_budget + accepted - 1, "limit": 1})
            assert budget_log["total"] == total_before_budget + accepted < 4096
            assert budget_log["entries"][0]["arguments"]["name"] == "B" * 62000
            assert rpc("ping")["alive"]
            rpc("shutdown")
            assert process.wait(timeout=10) == 0
            assert not session.exists() and not (project / ".engine/lock").exists()
            cli("entity", "create", project, "--name", "After live")
            # Restart rotates the credential and rejects the previous session's token.
            previous_credential = credential
            second = subprocess.Popen([engine, "live", "serve", str(project), "--json"],
                                      stdout=subprocess.PIPE, stderr=stderr, text=True)
            try:
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline and not session.exists() and second.poll() is None:
                    time.sleep(0.02)
                assert second.poll() is None and session.exists()
                rotated = json.loads(session.read_text())
                credential, port = rotated["token"], rotated["port"]
                known_credentials.append(credential)
                assert credential != previous_credential
                assert request("ping", credential_value=previous_credential) == denied
                assert rpc("ping")["alive"]
                # Small entries reach the independent4096-command ceiling.
                for _ in range(4096):
                    command("scene.validate@1")
                saturated = request("command", {"name": "scene.validate@1"})
                assert saturated["error"]["code"] == -32002
                assert rpc("inspect", {"target": "log"})["total"] == 4096
                rpc("shutdown")
                assert second.wait(timeout=10) == 0
            finally:
                if second.poll() is None:
                    second.kill()
                    second.wait(timeout=10)
            assert not session.exists() and not (project / ".engine/lock").exists()
            stderr.seek(0)
            assert credential not in stderr.read()
            print("Live API auth, bounds, disconnect/timeout/cap, lock exclusion, shared command log, sandbox and live-only asset acceptance passed")
            print("Command-log byte and4096-entry budgets, last record preservation and token rotation passed")
            completed = True
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
            if not completed:
                stderr.seek(0)
                diagnostic = stderr.read()
                for value in known_credentials:
                    diagnostic = diagnostic.replace(value, "<redacted>")
                print(diagnostic, file=sys.stderr)
            stderr.close()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
