#!/usr/bin/env python3
"""Exercise the VGA Start menu and Run dialog through a real QEMU PS/2 keyboard."""
import argparse
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

VGA_ADDRESS = "0xb8000"
VGA_BYTES = 80 * 25 * 2


class QemuMonitor:
    def __init__(self, connection):
        self.connection = connection
        self.read_prompt()

    def read_prompt(self, timeout=5.0):
        deadline = time.monotonic() + timeout
        received = bytearray()
        while b"(qemu) " not in received:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("QEMU monitor did not return its prompt")
            self.connection.settimeout(remaining)
            chunk = self.connection.recv(4096)
            if not chunk:
                raise RuntimeError("QEMU monitor disconnected")
            received.extend(chunk)
        return bytes(received)

    def command(self, command, timeout=5.0):
        self.connection.sendall(command.encode("ascii") + b"\n")
        return self.read_prompt(timeout).decode("utf-8", "replace")


def connect_monitor(path, process, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("QEMU exited before its monitor became ready")
        connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            connection.connect(path)
            return QemuMonitor(connection)
        except (FileNotFoundError, ConnectionRefusedError):
            connection.close()
            time.sleep(0.05)
    raise TimeoutError("QEMU monitor socket was not ready")


def read_vga(monitor, destination):
    destination.unlink(missing_ok=True)
    response = monitor.command(
        f'pmemsave {VGA_ADDRESS} {VGA_BYTES} "{destination.as_posix()}"'
    )
    if "Error" in response or not destination.is_file():
        raise RuntimeError(f"QEMU could not save VGA memory: {response.strip()}")
    raw = destination.read_bytes()
    if len(raw) != VGA_BYTES:
        raise RuntimeError(f"unexpected VGA dump size: {len(raw)}")
    characters = raw[0::2]
    return [
        characters[row * 80:(row + 1) * 80].decode("latin-1")
        for row in range(25)
    ]


def wait_for_screen(monitor, destination, process, predicate, description,
                    timeout):
    deadline = time.monotonic() + timeout
    last_rows = []
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"QEMU exited while waiting for {description}")
        last_rows = read_vga(monitor, destination)
        if predicate(last_rows):
            return last_rows
        time.sleep(0.1)
    visible = "\n".join(last_rows[:22])
    raise TimeoutError(f"timed out waiting for {description}\n{visible}")


def send_key(monitor, key, hold_ms=100):
    monitor.command(f"sendkey {key} {hold_ms}")


def send_text(monitor, text):
    for character in text:
        if not character.isascii() or not character.isalpha():
            raise ValueError(f"unsupported QEMU test character: {character!r}")
        send_key(monitor, character.lower(), 80)


def run(args):
    iso = Path(args.iso).resolve()
    if not iso.is_file():
        raise FileNotFoundError(f"ISO not found: {iso}")
    build_dir = Path(args.build_dir).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="qemu-start-menu-", dir=build_dir) as temp:
        monitor_path = os.path.join(temp, "monitor.sock")
        screen_path = Path(temp) / "vga.bin"
        command = [
            args.qemu,
            "-cdrom", str(iso),
            "-boot", "d",
            "-display", "none",
            "-serial", "none",
            "-monitor", f"unix:{monitor_path},server,nowait",
            "-no-reboot",
            "-no-shutdown",
        ]
        process = subprocess.Popen(
            command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        monitor = None
        try:
            monitor = connect_monitor(monitor_path, process, args.timeout)
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "AsterOS" in rows[0],
                "the AsterOS desktop", args.timeout,
            )

            # Exercise the actual Set-1 Ctrl+Esc path and menu renderer.
            send_key(monitor, "ctrl-esc")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "AsterOS Start" in rows[14] and "Run..." in rows[21],
                "the Start menu", 3.0,
            )

            # Select Run, submit an invalid command, then cancel without
            # disturbing the desktop or foreground application.
            for _ in range(6):
                send_key(monitor, "down", 80)
            send_key(monitor, "ret")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "Run AsterOS application" in rows[9],
                "the Run dialog", 3.0,
            )
            send_text(monitor, "bogus")
            send_key(monitor, "ret")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "Unknown app" in rows[14],
                "safe rejection of an unknown Run command", 3.0,
            )
            send_key(monitor, "esc")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "Select an application" in rows[5],
                "Run cancellation", 3.0,
            )

            # Launch Files through Run and verify Tab switches to Programs.
            send_key(monitor, "ctrl-esc")
            for _ in range(6):
                send_key(monitor, "down", 80)
            send_key(monitor, "ret")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "Run AsterOS application" in rows[9],
                "the second Run dialog", 3.0,
            )
            send_text(monitor, "files")
            send_key(monitor, "ret")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "Files" in rows[4],
                "launch of the Files application", 3.0,
            )
            send_key(monitor, "tab")
            wait_for_screen(
                monitor, screen_path, process,
                lambda rows: "Programs" in rows[4],
                "taskbar-style application switching with Tab", 3.0,
            )
            print("qemu start-menu smoke: Start, Run rejection/cancel, Files launch, and Tab switching passed")
        finally:
            if monitor is not None:
                monitor.connection.close()
            process.terminate()
            try:
                process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-i386")
    parser.add_argument("--iso", required=True)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--timeout", type=float, default=12.0)
    args = parser.parse_args()
    try:
        run(args)
    except Exception as error:
        print(f"qemu start-menu smoke: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
