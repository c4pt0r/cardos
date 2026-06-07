#!/usr/bin/env python3
"""cardos-app: push / list / delete / run Lua apps on a CardOS device over USB serial.

Protocol (see src/core/SerialControl.cpp): line commands; device replies are
prefixed "#CTRL# " so firmware log lines can be ignored. PUT streams raw bytes
after a READY, verified with CRC32.

Usage:
    tools/cardos-app.py push apps/hello.lua
    tools/cardos-app.py list
    tools/cardos-app.py rm hello.lua
    tools/cardos-app.py run hello.lua
    tools/cardos-app.py --selftest          # no device needed

Options: --port /dev/cu.usbmodemXXXX  --baud 115200
"""
import argparse
import glob
import os
import sys
import time
import zlib

CTRL = "#CTRL# "


def app_name_for(path, override=None):
    name = override or os.path.basename(path)
    if not name.endswith(".lua"):
        name += ".lua"
    return name


def valid_app_name(name):
    return (
        5 <= len(name) <= 48
        and "/" not in name
        and ".." not in name
        and name.endswith(".lua")
    )


def autodetect_port():
    pats = ["/dev/cu.usbmodem*", "/dev/tty.usbmodem*", "/dev/ttyACM*", "/dev/ttyUSB*"]
    for p in pats:
        hits = sorted(glob.glob(p))
        if hits:
            return hits[0]
    return None


class Client:
    def __init__(self, port, baud=115200, timeout=2.0):
        import serial  # lazy: only needed for real device ops

        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = timeout
        # Native USB CDC: avoid toggling DTR/RTS (no auto-reset, but be safe).
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        time.sleep(0.2)
        self.ser.reset_input_buffer()
        self._settle()

    def _settle(self):
        """Opening the CDC port can reboot the board; ping until it answers."""
        deadline = time.time() + 10.0
        while time.time() < deadline:
            self.ser.reset_input_buffer()
            self.send_line("PING")
            if self.read_ctrl(time.time() + 1.0) == "PONG":
                return
        sys.exit("device did not answer PING after port open")

    def close(self):
        self.ser.close()

    def send_line(self, line):
        self.ser.write((line + "\n").encode())
        self.ser.flush()

    def read_ctrl(self, deadline):
        """Return the next '#CTRL#' reply (without prefix), or None on timeout."""
        while time.time() < deadline:
            raw = self.ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode(errors="replace").rstrip("\r\n")
            except Exception:
                continue
            if line.startswith(CTRL):
                return line[len(CTRL):]
        return None

    def command(self, line, terminators=("OK", "ERR", "END", "PONG"), timeout=4.0):
        """Send a command, collect #CTRL# lines until a terminator."""
        self.ser.reset_input_buffer()
        self.send_line(line)
        deadline = time.time() + timeout
        out = []
        while True:
            r = self.read_ctrl(deadline)
            if r is None:
                raise TimeoutError(f"no reply to '{line.split()[0]}'")
            out.append(r)
            head = r.split(" ", 1)[0]
            if head in terminators:
                return out

    def ping(self):
        return self.command("PING", timeout=3.0)[-1] == "PONG"


def do_push(client, path, override):
    with open(path, "rb") as f:
        data = f.read()
    name = app_name_for(path, override)
    if not valid_app_name(name):
        sys.exit(f"invalid app name: {name} (must end .lua, no '/' or '..')")
    crc = zlib.crc32(data) & 0xFFFFFFFF
    client.ser.reset_input_buffer()
    client.send_line(f"PUT {name} {len(data)} {crc:08x}")
    deadline = time.time() + 5.0
    r = client.read_ctrl(deadline)
    while r is not None and r != "READY" and not r.startswith("ERR"):
        r = client.read_ctrl(deadline)
    if r is None:
        sys.exit("no READY from device")
    if r.startswith("ERR"):
        sys.exit(f"device rejected PUT: {r}")
    # Stream the payload throttled: the device-side reader drains in a
    # tight loop, but pacing keeps the CDC RX buffer comfortable.
    chunk = 128
    sent = 0
    while sent < len(data):
        client.ser.write(data[sent:sent + chunk])
        client.ser.flush()
        sent += chunk
        pct = min(100, sent * 100 // len(data))
        print(f"\r  uploading {name}: {pct}%", end="", flush=True)
        time.sleep(0.01)
    print()
    # Outlast the device's own 8s inter-byte timeout so a failure surfaces
    # as its ERR message instead of a silent None.
    final = client.read_ctrl(time.time() + 12.0)
    if final == "OK":
        print(f"OK: pushed {name} ({len(data)} bytes, crc {crc:08x})")
    else:
        sys.exit(f"upload failed: {final}")


def do_list(client):
    lines = client.command("LIST")
    apps = [l[len("ITEM "):] for l in lines if l.startswith("ITEM ")]
    if not apps:
        print("(no apps installed)")
        return
    print(f"{'NAME':<28} SIZE")
    for a in apps:
        parts = a.rsplit(" ", 1)
        name, size = (parts[0], parts[1]) if len(parts) == 2 else (a, "?")
        print(f"{name:<28} {size}")


def do_simple(client, verb, name):
    lines = client.command(f"{verb} {name}")
    last = lines[-1]
    if last == "OK":
        print(f"OK: {verb.lower()} {name}")
    else:
        sys.exit(f"{verb.lower()} failed: {last}")


def selftest():
    assert zlib.crc32(b"123456789") & 0xFFFFFFFF == 0xCBF43926
    assert valid_app_name("hello.lua")
    assert not valid_app_name("hello.txt")
    assert not valid_app_name("../x.lua")
    assert not valid_app_name("a/b.lua")
    assert app_name_for("apps/hello.lua") == "hello.lua"
    assert app_name_for("foo", "bar") == "bar.lua"
    print("selftest OK")


def main():
    ap = argparse.ArgumentParser(description="Manage CardOS Lua apps over serial.")
    ap.add_argument("--port", help="serial port (default: autodetect)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--selftest", action="store_true", help="run offline checks and exit")
    sub = ap.add_subparsers(dest="cmd")
    p = sub.add_parser("push"); p.add_argument("file"); p.add_argument("--name")
    sub.add_parser("list")
    p = sub.add_parser("rm"); p.add_argument("name")
    p = sub.add_parser("run"); p.add_argument("name")
    args = ap.parse_args()

    if args.selftest:
        selftest()
        return
    if not args.cmd:
        ap.print_help()
        sys.exit(1)

    port = args.port or autodetect_port()
    if not port:
        sys.exit("no serial port found; pass --port")
    print(f"port: {port}")
    client = Client(port, args.baud)
    try:
        if not client.ping():
            sys.exit("device did not respond to PING (is CardOS running?)")
        if args.cmd == "push":
            do_push(client, args.file, args.name)
        elif args.cmd == "list":
            do_list(client)
        elif args.cmd == "rm":
            do_simple(client, "DEL", args.name)
        elif args.cmd == "run":
            do_simple(client, "RUN", args.name)
    finally:
        client.close()


if __name__ == "__main__":
    main()
