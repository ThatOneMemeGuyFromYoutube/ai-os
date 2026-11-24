#!/usr/bin/env python3
"""Exercise AsterOS ARP and IPv4 ICMP receive/transmit through a QEMU socket peer."""

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
ETHERTYPE_IPV4 = b"\x08\x00"
ICMP_DATA = b"AsterOS ping"


def reserve_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def internet_checksum(data: bytes) -> int:
    if len(data) & 1:
        data += b"\x00"
    total = sum(struct.unpack_from("!H", data, index)[0]
                for index in range(0, len(data), 2))
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def build_arp_request() -> bytes:
    packet = bytearray(60)
    packet[0:6] = BROADCAST_MAC
    packet[6:12] = PEER_MAC
    packet[12:14] = ETHERTYPE_ARP
    packet[14:16] = b"\x00\x01"
    packet[16:18] = b"\x08\x00"
    packet[18:20] = b"\x06\x04"
    packet[20:22] = b"\x00\x01"
    packet[22:28] = PEER_MAC
    packet[28:32] = PEER_IP
    packet[38:42] = LOCAL_IP
    return bytes(packet)


def is_expected_arp_reply(frame: bytes) -> bool:
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


def build_icmp_echo_request(guest_mac: bytes) -> bytes:
    icmp = bytearray(8 + len(ICMP_DATA))
    icmp[0:2] = b"\x08\x00"
    icmp[4:8] = b"\x12\x34\x00\x01"
    icmp[8:] = ICMP_DATA
    struct.pack_into("!H", icmp, 2, internet_checksum(icmp))
    ip = bytearray(20)
    ip[0] = 0x45
    struct.pack_into("!H", ip, 2, len(ip) + len(icmp))
    struct.pack_into("!H", ip, 4, 0x1234)
    struct.pack_into("!H", ip, 6, 0x4000)
    ip[8] = 64
    ip[9] = 1
    ip[12:16] = PEER_IP
    ip[16:20] = LOCAL_IP
    struct.pack_into("!H", ip, 10, internet_checksum(ip))
    frame = bytearray(14) + ip + icmp
    frame[0:6] = guest_mac
    frame[6:12] = PEER_MAC
    frame[12:14] = ETHERTYPE_IPV4
    if len(frame) < 60:
        frame.extend(b"\x00" * (60 - len(frame)))
    return bytes(frame)


def is_expected_icmp_reply(frame: bytes, guest_mac: bytes) -> bool:
    if len(frame) < 42 or frame[12:14] != ETHERTYPE_IPV4:
        return False
    if frame[0:6] != PEER_MAC or frame[6:12] != guest_mac:
        return False
    ip = frame[14:]
    if len(ip) < 20 or ip[0] >> 4 != 4:
        return False
    header_length = (ip[0] & 0x0F) * 4
    if header_length < 20 or header_length > len(ip):
        return False
    total_length = struct.unpack_from("!H", ip, 2)[0]
    if total_length < header_length + 8 or total_length > len(ip):
        return False
    if ip[9] != 1 or ip[8] == 0 or internet_checksum(ip[:header_length]) != 0:
        return False
    if ip[12:16] != LOCAL_IP or ip[16:20] != PEER_IP:
        return False
    icmp = ip[header_length:total_length]
    return (icmp[0:2] == b"\x00\x00"
            and internet_checksum(icmp) == 0
            and icmp[4:8] == b"\x12\x34\x00\x01"
            and icmp[8:] == ICMP_DATA)


def run_peer(qemu: str, iso: Path, timeout_seconds: float) -> None:
    port = reserve_port()
    command = [
        qemu, "-m", "64M", "-cdrom", str(iso), "-boot", "d",
        "-display", "none", "-serial", "none", "-monitor", "none",
        "-no-reboot", "-no-shutdown",
        "-netdev", f"socket,id=net0,listen=127.0.0.1:{port}",
        "-device", "ne2k_pci,netdev=net0",
    ]
    with tempfile.TemporaryDirectory(prefix="asteros-ne2k-peer-") as temp_dir:
        log_path = Path(temp_dir) / "qemu.log"
        with log_path.open("w", encoding="utf-8") as log_file:
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=log_file)
            peer = None
            completed = False
            try:
                deadline = time.monotonic() + timeout_seconds
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited before the peer connected")
                    try:
                        peer = socket.create_connection(("127.0.0.1", port), timeout=0.25)
                        break
                    except OSError:
                        time.sleep(0.05)
                if peer is None:
                    raise TimeoutError("timed out connecting to QEMU socket netdev")
                peer.setblocking(False)
                receive_buffer = bytearray()
                phase = "arp"
                guest_mac = None
                next_request = 0.0
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited during the peer exchange")
                    now = time.monotonic()
                    if now >= next_request:
                        request = build_arp_request() if phase == "arp" else build_icmp_echo_request(guest_mac)
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
                            raise RuntimeError(f"QEMU sent an invalid Ethernet frame length: {frame_length}")
                        if len(receive_buffer) < frame_length + 4:
                            break
                        frame = bytes(receive_buffer[4:4 + frame_length])
                        del receive_buffer[:4 + frame_length]
                        if phase == "arp" and is_expected_arp_reply(frame):
                            guest_mac = frame[6:12]
                            phase = "icmp"
                            next_request = 0.0
                            print("NE2K peer smoke: guest received an ARP request and transmitted the matching ARP reply")
                        elif phase == "icmp" and is_expected_icmp_reply(frame, guest_mac):
                            print("NE2K peer smoke: guest received an IPv4 ICMP echo request and transmitted the matching echo reply")
                            completed = True
                            return
                raise TimeoutError(f"guest did not complete the {phase.upper()} peer exchange before the deadline")
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
    parser.add_argument("--timeout", type=float, default=20.0)
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
