#!/usr/bin/env python3
"""
make_ota.py — wrap a Telink TLSR8258 firmware .bin into a Zigbee OTA image.

Does two things, matching the Telink SDK's OTA scheme (same transform as the
romasku/tuya-zigbee-switch tooling, but with no third-party deps):

  1. If the .bin is not already stamped, pad to a multiple of 16, patch the
     firmware length field, add the Telink OTA magic (0x5D 0x02 at offset 6),
     and append a CRC32.
  2. Prepend the standard Zigbee OTA Upgrade file header + one sub-element.

Usage:
  make_ota.py <in.bin> <out.ota> --manufacturer-id 0x1141 \
      --image-type 0x0641 --file-version 0x10013001 --header-string "IH-K663"
"""
import argparse
import binascii
import struct

ZIGBEE_OTA_MAGIC = 0x0BEEF11E
TELINK_OTA_MAGIC = b"\x5d\x02"

OTA_HDR = struct.Struct("<I5HIH32sI")   # 56 bytes
OTA_SUB = struct.Struct("<HI")          # 6 bytes


def stamp_telink(image: bytes) -> bytes:
    if image[6:8] == TELINK_OTA_MAGIC:
        return image
    pad = (16 - len(image) % 16) % 16
    image = bytearray(image + b"\xff" * pad)
    image[0x18:0x1C] = (len(image) + 4).to_bytes(4, "little")  # fw length
    image[6:8] = TELINK_OTA_MAGIC
    crc = binascii.crc32(image) ^ 0xFFFFFFFF
    image += crc.to_bytes(4, "little")
    return bytes(image)


def make_ota(image: bytes, manuf: int, image_type: int, file_version: int,
             header_string: str) -> bytes:
    image = stamp_telink(image)
    hs = header_string.encode()
    if len(hs) > 31:
        raise SystemExit("header-string too long (max 31)")
    total = OTA_HDR.size + OTA_SUB.size + len(image)
    header = OTA_HDR.pack(
        ZIGBEE_OTA_MAGIC, 0x0100, OTA_HDR.size, 0,
        manuf, image_type, file_version, 2,
        hs + b"\x00" * (32 - len(hs)), total,
    )
    sub = OTA_SUB.pack(0, len(image))   # sub-element id 0 = upgrade image
    return header + sub + image


def int0(v: str) -> int:
    return int(v, 0)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("input")
    p.add_argument("output")
    p.add_argument("--manufacturer-id", type=int0, default=0x1141)
    p.add_argument("--image-type", type=int0, required=True)
    p.add_argument("--file-version", type=int0, required=True)
    p.add_argument("--header-string", default="Telink OTA Image")
    a = p.parse_args()
    with open(a.input, "rb") as f:
        image = f.read()
    ota = make_ota(image, a.manufacturer_id, a.image_type, a.file_version,
                   a.header_string)
    with open(a.output, "wb") as f:
        f.write(ota)
    print(f"wrote {a.output} ({len(ota)} bytes, fw {len(image)} bytes)")


if __name__ == "__main__":
    main()
