#!/usr/bin/env python3
"""Send one ESP32-C6 app image to the factory C6 UART updater."""

import argparse
import hashlib
import re
import sys
import time


CHUNK_BYTES = 1024
MAX_IMAGE_BYTES = 1920 * 1024
READY_TIMEOUT = 15.0
ACK_TIMEOUT = 5.0
RESULT_TIMEOUT = 90.0
REBOOT_TIMEOUT = 40.0
VERSION_TIMEOUT = 10.0


class FactoryReader:
    def __init__(self, serial_port):
        self.serial_port = serial_port
        self.buffered = bytearray()

    def wait_for(self, pattern, timeout, update_errors=True):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while b"\n" in self.buffered:
                line, _, self.buffered = self.buffered.partition(b"\n")
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                print(text)
                if pattern.search(text):
                    return text
                if update_errors and "FACTORY C6_UPDATE ERROR " in text:
                    raise RuntimeError(text)
            data = self.serial_port.read(self.serial_port.in_waiting or 1)
            if data:
                self.buffered.extend(data)
        raise TimeoutError("timed out waiting for " + pattern.pattern)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="factory P4 UART port")
    parser.add_argument("--baud", type=int, default=2_000_000, help="UART baud rate (default: 2000000)")
    parser.add_argument("--bundled", action="store_true", help="install the C6 image bundled in factory SPIFFS")
    parser.add_argument("image", nargs="?", help="ESP32-C6 OTA application .bin")
    args = parser.parse_args()
    if args.bundled == (args.image is not None):
        parser.error("provide an image or use --bundled, but not both")

    try:
        import serial
    except ImportError:
        print("pyserial is required: python -m pip install pyserial", file=sys.stderr)
        return 2

    image = None
    if args.bundled:
        command = "factory_c6_install\n"
    else:
        try:
            with open(args.image, "rb") as source:
                image = source.read()
        except OSError as exc:
            print("cannot read image: " + str(exc), file=sys.stderr)
            return 2
        if not image or len(image) > MAX_IMAGE_BYTES:
            print("image must be non-empty and at most %d bytes" % MAX_IMAGE_BYTES, file=sys.stderr)
            return 2
        digest = hashlib.sha256(image).hexdigest()
        command = "factory_c6_update %d %s\n" % (len(image), digest)
    try:
        with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=5) as serial_port:
            # Do not alter RTS/DTR: this CH343 link uses their normal defaults for auto-reset.
            serial_port.reset_input_buffer()
            reader = FactoryReader(serial_port)
            serial_port.write(command.encode("ascii"))
            serial_port.flush()
            if image is not None:
                reader.wait_for(re.compile(r"FACTORY C6_UPDATE READY bytes=%d chunk=%d" % (len(image), CHUNK_BYTES)), READY_TIMEOUT)
                for offset in range(0, len(image), CHUNK_BYTES):
                    chunk = image[offset:offset + CHUNK_BYTES]
                    serial_port.write(chunk)
                    serial_port.flush()
                    expected_offset = offset + len(chunk)
                    reader.wait_for(re.compile(r"FACTORY C6_UPDATE ACK offset=%d" % expected_offset), ACK_TIMEOUT)
            result = reader.wait_for(
                re.compile(r"FACTORY C6_UPDATE RESULT success bytes=[0-9]+ version=([0-9]+)\.([0-9]+)\.([0-9]+) rebooting=yes"),
                RESULT_TIMEOUT,
            )
            target_match = re.search(r"version=([0-9]+)\.([0-9]+)\.([0-9]+)", result)
            target_version = tuple(int(part) for part in target_match.groups())
            reader.wait_for(re.compile(r"FACTORY READY commands=factory_help"), REBOOT_TIMEOUT, update_errors=False)
            serial_port.write(b"factory_c6_version\n")
            serial_port.flush()
            version_line = reader.wait_for(
                re.compile(r"FACTORY C6_VERSION result=([^ ]+) major=([0-9]+) minor=([0-9]+) patch=([0-9]+)"),
                VERSION_TIMEOUT,
                update_errors=False,
            )
            version_match = re.search(r"result=([^ ]+) major=([0-9]+) minor=([0-9]+) patch=([0-9]+)", version_line)
            actual_result = version_match.group(1)
            actual_version = tuple(int(version_match.group(index)) for index in range(2, 5))
            if actual_result != "ESP_OK" or actual_version != target_version:
                raise RuntimeError("post-reboot C6 version result=%s actual=%d.%d.%d expected=%d.%d.%d" %
                                   ((actual_result,) + actual_version + target_version))
    except (OSError, RuntimeError, TimeoutError) as exc:
        print("C6 update failed: " + str(exc), file=sys.stderr)
        return 1
    print("C6 upgrade verified at %d.%d.%d." % target_version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
