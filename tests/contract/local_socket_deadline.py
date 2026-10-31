#!/usr/bin/env python3
"""An expired queued request must not dispatch after another handler blocks."""
from __future__ import annotations

import contextlib
import socket
import subprocess
import sys
import time


def main():
    with subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as process:
        try:
            port = int(process.stdout.readline())

            def connect():
                return socket.create_connection(("127.0.0.1", port), timeout=5)

            def closed(connection):
                try:
                    return connection.recv(64) == b""
                except (ConnectionResetError, ConnectionAbortedError):
                    return True

            with contextlib.ExitStack() as connections:
                gate, slow, pending = [connections.enter_context(connect()) for _ in range(3)]
                # A later accepted probe proves all earlier connections were accepted.
                with connect() as probe:
                    probe.sendall(b"probe\n")
                    assert probe.recv(64) == b"ok\n"
                    assert process.stdout.readline().strip() == "probe"
                gate.sendall(b"gate\n")
                assert process.stdout.readline().strip() == "gate"
                slow.sendall(b"slow\n")
                pending.sendall(b" ")
                process.stdin.write("release gate\n")
                process.stdin.flush()
                # Both slow and pending were ready in the same select snapshot.
                assert process.stdout.readline().strip() == "slow"
                time.sleep(1.2)  # Pending's absolute 1000ms receive deadline expires.
                pending.sendall(b"expired\n")
                process.stdin.write("release slow\n")
                process.stdin.flush()
                assert closed(pending), "expired pending frame was dispatched"
                assert slow.recv(64) == b"ok\n", "accepted handler must still complete"
                with connect() as shutdown:
                    shutdown.sendall(b"shutdown\n")
                    assert shutdown.recv(64) == b"ok\n"
                assert process.stdout.readline().strip() == "shutdown"
            assert process.wait(timeout=5) == 0
            print("Queued receive deadline survives a blocking handler; expired frame never dispatches")
            return 0
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
