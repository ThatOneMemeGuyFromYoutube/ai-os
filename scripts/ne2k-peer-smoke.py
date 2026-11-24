#!/usr/bin/env python3
"""Exercise AsterOS NE2K receive and transmit paths through a QEMU socket peer."""

import argparse
import select
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

LOCAL_IP = bytes((192, 0, 2, 2))
PEER_IP = bytes((192, 0, 2, 1))
PEER_MAC = bytes((0x52, 0x54, 0x00, 0xAA, 0xBB, 0xCC))
BROADCAST_MAC = b"\xff" * 6
ETHERTYPE_ARP = b"\x08\x06"


def reserve_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def build_arp_request() -> bytes:
    packet = bytearray(60)
    packet[0:6] = BROADCAST_MAC
    packet[6:12] = PEER_MAC
    packet[12:14] = ETHERTYPE_ARP
    packet[14:16] = b"\x00\x01"  # Ethernet hardware.
    packet[16:18] = b"\x08\x00"  # IPv4 protocol.
    packet[18:20] = b"\x06\x04"  # Hardware/protocol address lengths.
    packet[20:22] = b"\x00\x01"  # ARP request.
    packet[22:28] = PEER_MAC
    packet[28:32] = PEER_IP
    packet[38:42] = LOCAL_IP
    return bytes(packet)


def is_expected_reply(frame: bytes) -> bool:
    if len(frame) < 42 or frame[12:14] != ETHERTYPE_ARP:
        return False
    packet = frame[14:]
    return (
        packet[0:2] == b"\x00\x01"
        and packet[2:4] == b"\x08\x00"
        and packet[4:6] == b"\x06\x04"
        and packet[6:8] == b"\x00\x02"
        and frame[0:6] == PEER_MAC
        and frame[6:12] == packet[8:14]
        and packet[14:18] == LOCAL_IP
        and packet[18:24] == PEER_MAC
        and packet[24:28] == PEER_IP
    )


def run_peer(qemu: str, iso: Path, timeout_seconds: float) -> None:
    port = reserve_port()
    command = [
        qemu,
        "-m", "64M",
        "-cdrom", str(iso),
        "-boot", "d",
        "-display", "none",
        "-serial", "none",
        "-monitor", "none",
        "-no-reboot",
        "-no-shutdown",
        "-netdev", f"socket,id=net0,listen=127.0.0.1:{port}",
        "-device", "ne2k_pci,netdev=net0",
    ]

    with tempfile.TemporaryDirectory(prefix="asteros-ne2k-peer-") as temp_dir:
        log_path = Path(temp_dir) / "qemu.log"
        with log_path.open("w", encoding="utf-8") as log_file:
            process = subprocess.Popen(
                command,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=log_file,
            )
            peer = None
            completed = False
            try:
                deadline = time.monotonic() + timeout_seconds
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited before the peer connected")
                    try:
                        peer = socket.create_connection(
                            ("127.0.0.1", port), timeout=0.25
                        )
                        break
                    except OSError:
                        time.sleep(0.05)
                if peer is None:
                    raise TimeoutError("timed out connecting to QEMU socket netdev")

                peer.setblocking(False)
                receive_buffer = bytearray()
                request = build_arp_request()
                next_request = 0.0
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited during the peer exchange")
                    now = time.monotonic()
                    if now >= next_request:
                        peer.sendall(struct.pack("!I", len(request)) + request)
                        next_request = now + 0.5

                    readable, _, _ = select.select([peer], [], [], 0.1)
                    if not readable:
                        continue
                    chunk = peer.recv(4096)
                    if not chunk:
                        raise RuntimeError("QEMU closed the network peer connection")
                    receive_buffer.extend(chunk)

                    while len(receive_buffer) >= 4:
                        frame_length = struct.unpack("!I", receive_buffer[:4])[0]
                        if frame_length < 14 or frame_length > 1518:
                            raise RuntimeError(
                                f"QEMU sent an invalid Ethernet frame length: {frame_length}"
                            )
                        if len(receive_buffer) < frame_length + 4:
                            break
                        frame = bytes(receive_buffer[4:4 + frame_length])
                        del receive_buffer[:4 + frame_length]
                        if is_expected_reply(frame):
                            print(
                                "NE2K peer smoke: guest received an ARP request "
                                "and transmitted the matching ARP reply "
                                f"from {frame[6:12].hex(':')}"
                            )
                            completed = True
                            return

                raise TimeoutError(
                    "guest did not return the expected ARP reply before the deadline"
                )
            finally:
                if peer is not None:
                    peer.close()
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                if not completed:
                    log_file.flush()
                    details = log_path.read_text(encoding="utf-8", errors="replace")
                    if details.strip():
                        print("QEMU log:\n" + details[-4000:], file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-i386")
    parser.add_argument("--iso", type=Path, default=Path("build/ai-os.iso"))
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args()

    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if not args.iso.is_file():
        parser.error(f"ISO does not exist: {args.iso}")
    try:
        run_peer(args.qemu, args.iso, args.timeout)
    except (OSError, RuntimeError, TimeoutError) as error:
        print(f"NE2K peer smoke failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
